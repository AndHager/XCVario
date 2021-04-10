/*
 * Wind.cpp
 *
 *  Created on: Mar 21, 2021
 *
 *  Author: Eckhard Völlm, Axel Pauli
 *
 *  Last update: 2021-04-10
 */

#include <algorithm>
#include <cmath>
#include "esp_system.h"
#include "Compass.h"
#include "Flarm.h"
#include "Wind.h"

// TAS +- observation interval in Km/h
#define OI_TAS 10.0

// Heading +- observation interval in degrees
#define OI_HD 5.0

// Wait time in seconds after measurement start after which the wind will be
// calculated and delivered
#define DeliverWindAfter 10

// C-Function from sensor.cpp
extern "C" {
  float getTAS();
};

Wind::Wind() :
    _drift(0),
    nunberOfSamples( 0 ),
    measurementDuration( 0 ),
    deliverWind( DeliverWindAfter ),
    deltaSpeed( OI_TAS ), // +- 10 km/h
    deltaHeading( OI_HD ), // +- 5 degree
    tas( 0.0 ),
    groundSpeed( 0.0 ),
    trueCourse( 0.0 ),
    trueHeading( -1.0 ),
    sumTas( 0.0 ),
    sumGroundSpeed( 0.0 ),
    sumTHDeviation( 0.0 ),
    sumTCDeviation( 0.0 ),
    hMin( 0.0 ),
    hMax( 0.0 ),
    windDir( -1.0 ),
    windSpeed( -1.0 )
{
}

#if 0
double Wind::windspeed( double tas, double gs, double truehead, double gndhead ) {
  drift( truehead, gndhead );
  double ws = sqrt( tas*tas + gs*gs + 2*tas*gs*cos( DEG_TO_RAD( _drift )));
  return ws;
}

double Wind::winddir( double tas, double windspeed, double truehead, double gndhead ) {
  double wd = truehead - RAD_TO_DEG( asinf( tas / ( windspeed/ sin( _drift ))));
  return wd;
}
#endif

/**
 * Starts a new wind measurement cycle.
 */
void Wind::start()
{
  if( Flarm::gpsStatus() == true ) {
      // Ground speed in Km/h
      groundSpeed = Flarm::getGndSpeedKnots() * 1.852;
      trueCourse = Flarm::getGndCourse();
    }
  else {
      // Mark values as invalid.
      groundSpeed = -1.0;
      trueCourse = -1.0;
      trueHeading = -1.0;
      return;
    }

  nunberOfSamples = 1;
  measurementDuration = getMsTime();
  tas = double( getTAS() );

  bool ok;
  trueHeading = Compass::trueHeading( &ok );

  if( ok == false ) {
      // Mark true heading as invalid
      trueHeading = -1.0;
      return;
  }

  sumTas = tas;
  sumGroundSpeed = groundSpeed;
  sumTCDeviation = 0.0;
  sumTHDeviation = 0.0;

  // Define limit of observation window
  hMin = trueHeading - deltaHeading;

  windDir = -1.0;
  windSpeed = -1.0;
}

/**
 * Measurment cycle for wind calculation in straight flight. Should be
 * triggered periodically, maybe once per second.
 *
 * Returns true, if a new wind was calculated.
 */
bool Wind::calculateWind()
{
  bool ok = false;

  // Check GPRMC data status, GS, TC and TH
  if( Flarm::gpsStatus() == false || groundSpeed == -1.0 ||
      trueCourse == -1.0 || trueHeading == -1.0 ) {
      // GPS status or GS, TC and TH start data are not valid
      start();
      return false;
  }

  // Get current ground speed in km/h
  double cgs = Flarm::getGndSpeedKnots() * 1.852;

  // Check, if we have a GS value > 25 km/h. GS can be nearly zero in the wave.
  // If GS is to low, the measurement make no sense.
  if( cgs < 25.0 )
    {
      // We start a new measurement cycle.
      start();
      return false;
    }

  // Check, if given ground speed deltas are valid.
  if( fabs( groundSpeed - cgs ) > deltaSpeed )
    {
      // Condition violated, start a new measurements cycle.
      start();
      return false;
    }

  // Get current TAS in km/h
  double ctas = double( getTAS() );

  // check if given TAS deltas are valid.
  if( fabs( tas - ctas ) > deltaSpeed )
    {
      // Condition violated, start a new measurements cycle.
      start();
      return false;
    }

  // Get current true heading
  double cth = Compass::trueHeading( &ok );

  if( cth >= (360.0 - deltaHeading ) )
    {
      cth -= 360.0;
    }

  // Check if given magnetic heading deltas are valid.
  if( ok == false || ! ( cth >= hMin && cth <= hMax ) )
    {
      // Condition violated, start a new measurements cycle.
      start();
      return false;
    }

  // Get true course
  double ctc = Flarm::getGndCourse();

  if( ctc >= (360.0 - deltaHeading ) )
    {
      ctc -= 360.0;
    }

  // Check if given true course deltas are valid.
  if( ! ( ctc >= hMin && ctc <= hMax ) )
    {
      // Condition violated, start a new measurements cycle.
      start();
      return false;
    }

  // Take all as new sample
  nunberOfSamples++;

  // The given deltas are fulfilled, summarize values
  sumTas += ctas;
  sumGroundSpeed += cgs;

  // Calculate course deviations
  double deviation = ctc - trueCourse;

  if( deviation < -180. ) { deviation += 360.; }

  sumTCDeviation += deviation;

  deviation = cth - trueHeading;

  if( deviation < -180. ) { deviation += 360.; }

  sumTHDeviation += deviation;

  if( elapsed() >= deliverWind * 1000 )
    {
      /**
       calculate wind by using wind triangle, see more here:
       http://klspublishing.de/downloads/KLSP%20061%20Allgemeine%20Navigation%20DREHMEIER.pdf

       The Wind Correction Angle is the angle between the Heading and the
       Desired Course:

       WCA = Heading - DesiredCourse
       */
      double nos = double( nunberOfSamples );

      double tc = sumTCDeviation / nos;

      // normalize angle
      if (tc >= 360.) { tc -= 360.; }
      else if (tc < 0.) { tc += 360.; }

      tc += trueCourse; // Average of TC

      double th = sumTHDeviation / nos;

      // normalize angle
       if (th >= 360.) { th -= 360.; }
       else if (th < 0.) { th += 360.; }

       th += trueHeading; // Average of TH

      // WCA in radians
      double wca = (( sumTCDeviation - sumTHDeviation ) / nos ) * M_PI / 180.0;
      double tas = sumTas / nos;
      double gs = sumGroundSpeed / nos;

      // Apply the Cosinus sentence: c^2 = a^2 + b^2 − 2 * a * b * cos( α )
      // to calculate the WS (wind speed)
      double ws = sqrt( (tas * tas) + (gs * gs ) - ( 2 * tas * gs * cos( wca ) ) );

      // WS / sin(WCA)
      double term = ws / sin( wca );

      // calculate WA (wind angle) in degree
      double wa = asin( tas / term ) * 180. / M_PI;

      // Wind direction: W = TC - WA
      double wd = tc - wa;

      if( wd < 0 )
        {
          wd += 360.;
        }
      else if( wd >= 360. )
        {
          wd -= 360;
        }

      // store calculated results
      windSpeed = ws;// wind speed in km/h
      windDir = wd;  // wind direction in degrees
      return true;
    }

  return false;
}
