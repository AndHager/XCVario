#ifndef FLAP_H
#define FLAP_H

#include "Flap.h"
#include "AnalogInput.h"

/*
 * This class handels flap display and Flap sensor
 *
 */

class Flap {
public:
	static void  init();
	static float getLeverPosition( int sensorreading );
	static void  progress();
	static void  initSensor();
	static void  initSpeeds();
	static inline float getLever() { return lever; }
	static inline void setLever( float l ) { lever = l; }
	// recommendations
	static float getOptimum( float wks, int wki );
	static int   getOptimumInt( float wks );

	static inline unsigned int getSensorRaw() {
		if( haveSensor() )
			return sensorAdc->getRaw(1000);
		else
			return 0;
	}
	static inline bool haveSensor() { if( sensorAdc != 0 ) return true; else return false; }

private:
	static AnalogInput *sensorAdc;
	static float lever;
	static int   senspos[9];
	static int   leverold;
	static int   flapSpeeds[9];
};

#endif
