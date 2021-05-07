/*
 * Wind.cpp
 *
 *  Created on: Mar 21, 2021
 *
 *  Author: Eckhard Völlm, Axel Pauli
 *
 *  Last update: 2021-04-21
 */
#include <algorithm>
#include <cmath>
#include "esp_system.h"
#include "Compass.h"
#include "Flarm.h"
#include "SetupNG.h"
#include "Wind.h"
#include "Units.h"
#include "sensor.h"
#include "math.h"
#include "CircleWind.h"

// degree to rad conversion
#define D2R(x) ((x)/57.2957795131)
#define R2D(x) ((x)*57.2957795131)

Wind::Wind() :
nunberOfSamples( 0 ),
measurementStart( 0 ),
tas( 0.0 ),
groundSpeed( 0.0 ),
trueCourse( 0.0 ),
trueHeading( -1.0 ),
sumTas( 0.0 ),
sumGroundSpeed( 0.0 ),
averageTH( 0.0 ),
averageTC( 0.0 ),
averageGS(0.0),
tcMin( 0.0 ),
tcMax( 0.0 ),
mhMin( 0.0 ),
mhMax( 0.0 ),
windDir( -1.0 ),
windSpeed( -1.0 ),
lowAirspeed( false ),
circlingWindDir( -1.0 ),
circlingWindSpeed( -1.0 )
{
}

double Wind::meanAngleEckhard( double angle, double average ){
	bool neg=false;
	if( angle > 180 ){  // check if angle is in the second half and convert to -+ 180° representation in case
		angle = angle - 360;
		neg=true;
	}
	if( average > 180 ){
		average = average - 360;
		neg=true;
	}
	double radangle=D2R(angle);
	double radaverage=D2R(average);
	double result = R2D(atan2( sin(radangle)+sin(radaverage), cos(radangle)+cos(radaverage)));
	if( neg ) // convert back from -+180° representation
		result = result + 360;
	if( result > 360 )
		result = result - 360;
	return result;
}

/**
 * Calculate the smaller bisector value from angles.
 *
 * @param angle as degree 0...359
 * @param average as degree 0...359
 * @return average angle as degree 0...359
 */
double Wind::meanAngle( double angle, double average )
{
	double bisector = 0.0;
	double result = 0.0;
	double absDiff = fabs( angle - average );

	if( absDiff > 180.0 ) {
		bisector = ( 360.0 - absDiff ) / 2.0;

		if( angle <= average ) {
			result = average + bisector;
		}
		else {
			result = average - bisector;
		}
	}
	else {
		bisector = absDiff / 2.0;

		if( angle <= average ) {
			result = angle + bisector;
		}
		else {
			result = angle - bisector;
		}
	}
	return normAngle( result );
}

/**
 * Starts a new wind measurement cycle.
 */
void Wind::start()
{
	if( Flarm::gpsStatus() == true ) {
		// Ground speed in Km/h
		groundSpeed = Units::knots2kmh(Flarm::getGndSpeedKnots());
		trueCourse = Flarm::getGndCourse();
	}
	else {
		// Mark all values as invalid.
		groundSpeed = -1.0;
		trueCourse = -1.0;
		trueHeading = -1.0;
		return;
	}

	nunberOfSamples = 1;
	measurementStart = getMsTime();
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

	averageTC = trueCourse;
	averageTH = trueHeading;

	// Define limit of TH observation window
	mhMin = normAngle( trueHeading - wind_heading_delta.get() );
	mhMax = normAngle( trueHeading + wind_heading_delta.get() );

	// Define limit of TC observation window
	tcMin = normAngle( trueCourse - wind_heading_delta.get() );
	tcMax = normAngle( trueCourse + wind_heading_delta.get() );
}

/**
 * Measurement cycle for wind calculation in straight flight. Should be
 * triggered periodically, maybe once per second.
 *
 * Returns true, if a new wind was calculated.
 */
bool Wind::calculateWind()
{
	// ESP_LOGI(FNAME,"calculateWind");
	// Check if wind requirements are fulfilled
	if( compass_enable.get() == false || compass_calibrated.get() == false || wind_enable.get() == false ) {
		return false;
	}

	// Check GPRMC data status, GS, TC and TH
	if( Flarm::gpsStatus() == false || groundSpeed == -1.0 ||
			trueCourse == -1.0 || trueHeading == -1.0 ) {
		// GPS status or GS, TC and TH start data are not valid
		start();
		ESP_LOGI(FNAME,"Restart Cycle: GPS Status bad or GS invalid");
		return false;
	}

	// Get current ground speed in km/h
	double cgs = Units::knots2kmh( Flarm::getGndSpeedKnots() );

	// Check, if given ground speed deltas are valid.
	if( fabs( groundSpeed - cgs ) > Units::Airspeed2Kmh( wind_speed_delta.get() ) ) {
		// Condition violated, start a new measurements cycle.
		start();
		ESP_LOGI(FNAME,"Restart Cycle GS %3.1f - CGS: %3.1f > %3.1f", groundSpeed, cgs, Units::Airspeed2Kmh( wind_speed_delta.get() ) );
		return false;
	}

	// Get current TAS in km/h
	double ctas = double( getTAS() );

	// check if given TAS deltas are valid.
	if( fabs( tas - ctas ) > Units::Airspeed2Kmh( wind_speed_delta.get() ) ) {
		// Condition violated, start a new measurements cycle.
		start();
		ESP_LOGI(FNAME,"TAS %3.1f - CTAS: %3.1f  > delta %3.1f", tas, ctas, Units::Airspeed2Kmh( wind_speed_delta.get() ) );
		return false;
	}

	// Check, if we have a AS value > minimum, default is 25 km/h.
	// If GS is nearly zero, the measurement makes also sense (wave), hence if we are not flying it doesn't
	if( ctas < Units::Airspeed2Kmh( wind_as_min.get() ) )
	{
		// We start a new measurement cycle.
		start();
		if( !lowAirspeed ) {
			ESP_LOGI(FNAME,"Low Airspeed, stop wind calculation, AS %3.1f  < %3.1f Kmh", ctas,  Units::Airspeed2Kmh( wind_as_min.get() ) );
			lowAirspeed = true;
		}
		return false;
	}else{
		if( lowAirspeed ) {
			ESP_LOGI(FNAME,"Airspeed OK, start wind calculation, AS %3.1f  < %3.1f Kmh", ctas,  Units::Airspeed2Kmh( wind_as_min.get() ) );
			lowAirspeed = false;
		}
	}

	// Get current true heading from compass.
	bool ok = true;
	double cth = Compass::trueHeading( &ok );

	if( ok == false ) {
		// No valid heading available
		start();
		return false;
	}

	// Check if given magnetic heading deltas are valid.
	if( mhMin < mhMax ){
		if( cth < mhMin || cth > mhMax )
			ok = false;  // Heading outside of observation window
	}
	else
	{
		if( cth < mhMax || cth > mhMin  )
			ok = false; // Heading outside of observation window
	}

	if( ok == false ) {
		// Condition violated, start a new measurements cycle.
		start();
		ESP_LOGI(FNAME,"Restart Cycle, CTH %3.1f outside min: %3.1f max %3.1f", cth, mhMin, mhMax );
		return false;
	}

	// Get current true course from GPS
	double ctc = Flarm::getGndCourse();

	if( cgs >= 10 ) {
		// The ground course check is only done, if the ground speed is >=10 Km/h.
		// Near speed zero, the ground course is not stable in its direction.
		// Check if given GPS true course deltas are valid.
		if( tcMin < tcMax && ( ctc < tcMin || cth > tcMax ) ) {
			// true course outside of observation window
			ok = false;
		}
		else if( tcMin > tcMax && cth < tcMin && cth > tcMax ) {
			// true course outside of observation window
			ok = false;
		}

		if( ok == false ) {
			// Condition violated, start a new measurements cycle.
			start();
			ESP_LOGI(FNAME,"Restart Cycle, Ground Heading CTC: %3.1f outside min: %3.1f max: %3.1f", ctc, tcMin, tcMax );
			return false;
		}
	}

	// Take all as new sample
	nunberOfSamples++;

	// The given deltas are fulfilled, summarize values
	sumTas += ctas;
	sumGroundSpeed += cgs;

	// Calculate average true course TC
	averageTC = averageTC + (meanAngle( ctc, averageTC ) - averageTC) * 0.1;

	// Calculate average true heading TH
	averageTH = averageTH + (meanAngle( cth, averageTH ) - averageTH) * 0.1;

	ESP_LOGI(FNAME,"%d TC: %3.1f (avg:%3.1f) GS:%3.1f TH: %3.1f (avg:%3.1f) IAS: %3.1f", nunberOfSamples, ctc, averageTC, cgs, cth, averageTH, ctas );
	// ESP_LOGI(FNAME,"avTC: %3.1f avTH:%3.1f ",averageTC,averageTH  );

	if( elapsed() >= wind_measurement_time.get() * 1000 )
	{
		/**
       calculate wind by using wind triangle, see more here:
       http://klspublishing.de/downloads/KLSP%20061%20Allgemeine%20Navigation%20DREHMEIER.pdf

       The Wind Correction Angle is the angle between the Heading and the
       Desired Course:

       WCA = Heading - DesiredCourse
		 */
		double nos = double( nunberOfSamples );
		double tc = normAngle( averageTC );

		double th = normAngle( averageTH );

		// WCA in radians
		double tas = sumTas / nos;
		averageGS = sumGroundSpeed / nos;

		calculateWind( tc, averageGS, th, tas  );
		measurementStart = getMsTime();  // it is enough to calculate every ten seconds a new wind
		return true;
	}
	return false;
}

// length (or speed) of third vector in windtriangle
double Wind::calculateSpeed( double angle1, double speed1, double angle2, double speed2  ){
	double delta = D2R( angle2 - angle1 );
	// Cosinus sentence: c^2 = a^2 + b^2 − 2 * a * b * cos( α ) for wind speed in km/h
	return sqrt( (speed2 * speed2) + (speed1 * speed1 ) - ( 2 * speed2 * speed1 * cos( delta ) ) );
}

// calculate WA (wind angle) in degree
// wind direction calculation taken from here:
// view-source:http://www.owoba.de/fliegerei/flugrechner.html
// direction in degrees of third vector in windtriangle
double Wind::calculateAngle( double angle1, double speed1, double angle2, double speed2  ){
	double tcrad = D2R( angle1 );
	double thrad = D2R( angle2 );

	double ang = tcrad + atan2( speed2 * sin( thrad - tcrad ), speed2 * cos( thrad - tcrad ) - speed1 );
	return( normAngle( R2D( ang ) ) );  // convert radian to degree
}

void Wind::calculateWind( double tc, double gs, double th, double tas  ){
	ESP_LOGI(FNAME,"calculateWind: TC:%3.1f GS:%3.1f TH:%3.1f TAS:%3.1f", tc, gs, th, tas );
	// Wind correction angle WCA
	if( gs < 5 ){
		tc = th;   // what will deliver heading and airspeed for wind
	}
	// Wind speed
	windSpeed = calculateSpeed( tc, gs, th, tas );
	// wind direction
	windDir = calculateAngle( tc, gs, th, tas );

	ESP_LOGI(FNAME,"New WindDirection: %3.1f deg,  Strength: %3.1f km/h", windDir, windSpeed  );

	// Reverse calculate windtriangle for deviation and airspeed calibration
	float airspeed = calculateSpeed( circlingWindDir, circlingWindSpeed, averageTH, averageGS );
	float trueHeading = calculateAngle( circlingWindDir, circlingWindSpeed, averageTH, averageGS );
	ESP_LOGI(FNAME,"New heading vector from Wind and GS: %3.1f°/%3.1f km/h Compass: %3.1f°", trueHeading, airspeed, averageTH  );
}


void Wind::newCirclingWind( float angle, float speed ){
	ESP_LOGI(FNAME,"New good circling wind %3.2f°/%3.2f", angle, speed );
	circlingWindDir = angle;
	circlingWindSpeed = speed;
}

void Wind::test()
{    // Class Test, check here the results: http://www.owoba.de/fliegerei/flugrechner.html
	calculateWind( 90, 100, 0, 100 );
	if( int( windSpeed ) != 141 || int(windDir +0.5) != 135 )
		ESP_LOGI(FNAME,"Failed");
	calculateWind( 270, 100, 0, 100 );
	if( int( windSpeed ) != 141 || int(windDir +0.5) != 225 )
		ESP_LOGI(FNAME,"Failed");

	calculateWind( 0, 100, 90, 100 );
	if( int( windSpeed ) != 141 || int(windDir +0.5) != 315 )
		ESP_LOGI(FNAME,"Failed");
	calculateWind( 0, 100, 270, 100 );
	if( int( windSpeed ) != 141 || int(windDir +0.5) != 45 )
		ESP_LOGI(FNAME,"Failed");

	calculateWind( 90, 100, 180, 100 );
	if( int( windSpeed ) != 141 || int(windDir +0.5) != 45 )
		ESP_LOGI(FNAME,"Failed");

	calculateWind( 180, 100, 270, 100 );
	if( int( windSpeed ) != 141 || int(windDir +0.5) != 135  )
		ESP_LOGI(FNAME,"Failed");
}
