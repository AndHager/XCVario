/*
 * Wind.h
 *
 *  Created on: Mar 21, 2021
 *
 *  Author: Eckhard Völlm, Axel Pauli
 *
 *  Last update: 2021-04-10
 */
#pragma once

#include <sys/time.h>

class Wind
{
public:
	Wind();
	virtual ~Wind() {};
	void drift( double truehead, double gndhead ) { _drift = truehead - gndhead; };

#if 0
	double winddir( double tas, double windspeed, double truehead, double gndhead );
	double windspeed( double tas, double gs, double truehead, double gndhead );
#endif

  /**
   * Get time in ms since 1.1.1970
   */
  static uint64_t getMsTime()
  {
    struct timeval tv;
    gettimeofday( &tv, nullptr );
    return ( tv.tv_sec * 1000 ) + ( tv.tv_usec / 1000 );
  }

  /**
   * Starts a new wind measurment cycle.
   */
  void start();

  /**
   * Measurement cycle for wind calculation in straight flight. Should be
   * triggered periodically, maybe once per second.
   *
   * Returns true, if a new wind was calculated.
   */
  bool calculateWind();

  /**
   * Return the last calculated wind. If return result is true, the wind data
   * are valid otherwise false.
   */
  bool getWind( int* direction, float* speed )
  {
    *direction = int( windDir + 0.5 );
    *speed = float( windSpeed );
    return ( windDir != -1.0 && windSpeed != -1.0 );
  }

private:

  /**
   * Return the elapsed time in milliseconds
   */
  uint64_t elapsed()
  {
    return getMsTime() - measurementDuration;
  }

	double _drift;

  uint16_t nunberOfSamples;      // current number of samples
  uint64_t measurementDuration;  // time measurement in milliseconds
  uint16_t deliverWind;          // Wait time before wind deliver
  double deltaSpeed;             // accepted speed deviation in km/h
  double deltaHeading;           // accepted heading deviation in degrees
  double tas;                    // TAS in km/h
  double groundSpeed;            // GS in km/h
  double trueCourse;             // Compass true heading
  double trueHeading;            // GPS heading
  double sumTas;                 // TAS in km/h
  double sumGroundSpeed;         // sum of GS in km/h
  double sumTHDeviation;         // sum of Compass true heading deviation
  double sumTCDeviation;         // sum of GPS heading (true course) deviation
  double hMin;                   // lower limit of heading observation window
  double hMax;                   // upper limit of heading observation window
  double windDir;                // calculated wind direction
  double windSpeed;              // calculated wind speed in Km/h
};
