/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************

File: CompassMenu.cpp

Class to handle compass data and actions.

Author: Axel Pauli, February 2021

Last update: 2021-02-26

 **************************************************************************/

#include <cstring>
#include <cstdlib>
#include <cmath>
#include "esp_log.h"
#include "esp_system.h"

#include "CompassMenu.h"
#include "MenuEntry.h"
#include "sensor.h"  // we need spiMutex
#include "vector.h"
#include "Colors.h"
#include "logdef.h"

SetupMenuSelect* CompassMenu::menuPtr = nullptr;

// Initialise static members
SetupNG<float>* CompassMenu::deviations[8] = {
		&compass_dev_0,
		&compass_dev_45,
		&compass_dev_90,
		&compass_dev_135,
		&compass_dev_180,
		&compass_dev_225,
		&compass_dev_270,
		&compass_dev_315 };
/**
 * Creates a compass menu instance with an active compass object.
 */
CompassMenu::CompassMenu( Compass& compassIn ) :
										 compass( compassIn )
{
}

CompassMenu::~CompassMenu()
{
}

// Compass Menu Action Routine
int CompassMenu::deviationAction( SetupMenuSelect *p )
{
	ESP_LOGI( FNAME, "Compass deviation setup for direction '%s'",	p->getEntry() );

	if( compass.haveSensor() == false )
	{
		p->clear();
		p->ucg->setFont( ucg_font_ncenR14_hr );
		p->ucg->setPrintPos( 1, 100 );
		p->ucg->printf( "No magnetic Sensor, Abort" );
		delay( 2000 );
		ESP_LOGI( FNAME, "Abort calibration, no sensor signal" );
		return 0;
	}

	short direction = strtol( p->getEntry(), nullptr, 10 );
	short diridx = direction / 45;

	// Calibration menu is requested
	const unsigned short skydirs[8] = { 0, 45, 90, 135, 180, 225, 270, 315 };

	p->clear();
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	p->ucg->setFont( ucg_font_ncenR14_hr );
	p->ucg->setPrintPos( 1, 60 );
	p->ucg->printf( "Turn airplane to %s  ", p->getEntry() );
	p->ucg->setPrintPos( 1, 90 );
	p->ucg->printf( "and push button when done" );
	xSemaphoreGive(spiMutex);
	delay( 500 );

	float heading = 0.0;
	float deviation = 0;

	while( !p->readSwitch() )
	{
		bool ok = true;
		if( heading == 0.0 )
			heading = Compass::rawHeading( &ok );
		else
			heading = heading + (Compass::rawHeading( &ok ) -heading)*0.05; // a bit low pass
		if( ok == false )
		{
			// in case of error deviation is set to 0
			heading = static_cast<float>( skydirs[diridx] );
		}
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		p->ucg->setPrintPos( 1, 180 );
		p->ucg->printf( "Heading:  %d° ", (int)(heading+0.5) );
		p->ucg->setPrintPos( 1, 230 );
		deviation = Vector::normalizeDeg( direction - heading );
		p->ucg->printf( "Deviation: %3.1f°  ", deviation );
		xSemaphoreGive(spiMutex);
		delay( 50 );
	}
	while( p->readSwitch() )
	{
		// wait so long while rotary is pressed to avoid unwanted actions
		delay( 50 );
		continue;
	}

	// Save deviation value
	deviations[diridx]->set( deviation );
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	p->ucg->setPrintPos( 1, 270 );
	p->ucg->printf( "Saved" );
	delay(500);
	p->ucg->setPrintPos( 1, 300 );
	p->_parent->highlight++;
	if(p->_parent->highlight > 7 )
		p->_parent->highlight = -1;
	p->ucg->printf( "Press key for next" );
	xSemaphoreGive(spiMutex);
	// Update compass interpolation data
	Compass::deviationReload();

	ESP_LOGI( FNAME, "Compass deviation action for %s is finished",	p->getEntry() );
	return 0;
}

/** Compass Menu Action method to reset all deviations to 0. */
int CompassMenu::resetDeviationAction( SetupMenuSelect *p )
{
	if( p->getSelect() == 0 )
	{
		// Cancel is selected
		return 0;
	}
	else if( p->getSelect() == 1 )
	{
		p->clear();
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		p->ucg->setFont( ucg_font_ncenR14_hr );
		p->ucg->setPrintPos( 1, 60 );
		p->ucg->printf( "Reset all compass" );
		p->ucg->setPrintPos( 1, 90 );
		p->ucg->printf( "deviation data" );
		xSemaphoreGive(spiMutex);
		// Reset calibration
		for( int i = 0; i < 8; i++ )
		{
			deviations[i]->set( 0.0 );
		}

		ESP_LOGI( FNAME, "All compass deviations values were reset" );
		delay( 1000 );
	}
	// Reset also AS calibration
	wind_as_calibration.set( 1.0 );
	// Reload compass interpolation data
	Compass::loadDeviationMap();
	Compass::deviationReload();

	p->clear();
	p->ucg->setFont( ucg_font_ncenR14_hr );
	p->ucg->setPrintPos( 1, 300 );
	p->ucg->printf( "Saved        " );
	delay( 2000 );
	return 0;
}

int CompassMenu::declinationAction( SetupMenuValFloat *p )
{
	ESP_LOGI( FNAME, "declinationAction()" );

	if( p->pressed )
	{
		ESP_LOGI( FNAME, "Compass declination set to valid" );
	}

	return 0;
}

int CompassMenu::sensorCalibrationAction( SetupMenuSelect *p )
{
	ESP_LOGI( FNAME, "sensorCalibrationAction()" );
	bool only_show = (p->getSelect() == 2);  // Show

	if( p->getSelect() == 0 )
	{
		// Cancel is requested
		ESP_LOGI( FNAME, "Cancel Button pressed" );
		return 0;
	}

	ESP_LOGI( FNAME, "Start magnetic sensor calibration" );

	menuPtr = p;

	p->clear();
	if( only_show ){
		compass.calibrate( calibrationReport, true );
		while( !p->readSwitch() )
			delay( 100 );
	}else{
		p->ucg->setFont( ucg_font_ncenR14_hr, true );
		p->ucg->setPrintPos( 1, 30 );
		p->ucg->printf( "Calibration is started" );
		p->ucg->setPrintPos( 1, 220 );
		p->ucg->printf( "Now rotate sensor until" );
		p->ucg->setPrintPos( 1, 245 );
		p->ucg->printf( "all numbers are green" );
		p->ucg->setPrintPos( 1, 270 );
		p->ucg->printf( "Press button to finish" );
		compass.calibrate( calibrationReport, false);
		p->ucg->setPrintPos( 1, 250 );
		delay( 1000 );
		p->clear();
	}

	menuPtr = nullptr;
	return 0;
}

float CompassMenu::x_back = 0;
float CompassMenu::y_back = 0;
float CompassMenu::z_back = 0;
float CompassMenu::xscale_back = 0;
float CompassMenu::yscale_back = 0;
float CompassMenu::zscale_back = 0;
float CompassMenu::xbias_back = 0;
float CompassMenu::ybias_back = 0;
float CompassMenu::zbias_back = 0;

static int xm,ym,zm = 0;
static int xi,yi,zi = 0;

static bitfield_compass bits_old = { false, false, false, false, false, false };

/** Method for receiving intermediate calibration results. */
bool CompassMenu::calibrationReport( float x, float y, float z, float xscale, float yscale, float zscale, float xbias, float ybias, float zbias, bitfield_compass b, bool print )
{
	if( menuPtr == nullptr )
		return false;
	xSemaphoreTake(spiMutex,portMAX_DELAY );

	// X
	if( b.xmax_green && b.xmin_green )
		menuPtr->ucg->setColor( COLOR_GREEN );
	else
		menuPtr->ucg->setColor( COLOR_WHITE );
	if( xscale != xscale_back || print || !(bits_old == b) ){
		menuPtr->ucg->setPrintPos( 1, 60 );
		menuPtr->ucg->printf( "X-Scale=%3.1f  ", xscale * 100 );
		xscale_back = xscale;
	}
	if( (x_back != x || !(bits_old == b)) && !print ){
		menuPtr->ucg->setPrintPos( 160, 60 );
		menuPtr->ucg->printf( "(%.1f)  ", x/32768 *100 );
		x_back = x;
	}
	if( xbias != xbias_back || print || !(bits_old == b) ){
		menuPtr->ucg->setPrintPos( 1, 135 );
		menuPtr->ucg->printf( "X-Bias=%3.1f  ", xbias/32768 *100 );
		xbias_back = xbias;
	}
	// Y
	if( b.ymax_green && b.ymin_green )
		menuPtr->ucg->setColor( COLOR_GREEN );
	else
		menuPtr->ucg->setColor( COLOR_WHITE );
	if(yscale != yscale_back || print || !(bits_old == b) ){
		menuPtr->ucg->setPrintPos( 1, 85 );
		menuPtr->ucg->printf( "Y-Scale=%3.1f  ", yscale * 100);
		yscale_back = yscale;
	}
	if( (y_back != y || !(bits_old == b) ) && !print  ){
		menuPtr->ucg->setPrintPos( 160, 85 );
		menuPtr->ucg->printf( "(%.1f)  ", y/32768 *100 );
		y_back = y;
	}
	if( ybias != ybias_back || print || !(bits_old == b) ){
		menuPtr->ucg->setPrintPos( 1, 160 );
		menuPtr->ucg->printf( "Y-Bias=%3.1f  ", ybias/32768 *100 );
		ybias_back = ybias;
	}
	// Z
	if( b.zmax_green && b.zmin_green )
		menuPtr->ucg->setColor( COLOR_GREEN );
	else
		menuPtr->ucg->setColor( COLOR_WHITE );
	if( zscale != zscale_back || print || !(bits_old == b) ){
		menuPtr->ucg->setPrintPos( 1, 110 );
		menuPtr->ucg->printf( "Z-Scale=%3.1f  ", zscale * 100 );
		zscale_back = zscale;
	}
	if( (z_back != z || !(bits_old == b)) && !print ){
		menuPtr->ucg->setPrintPos( 160, 110 );
		menuPtr->ucg->printf( "(%.1f)  ", z/32768 *100 );
		z_back = z;
	}
	if( zbias != zbias_back || print || !(bits_old == b) ){
		menuPtr->ucg->setPrintPos( 1, 185 );
		menuPtr->ucg->printf( "Z-Bias=%3.1f  ", zbias/32768 *100 );
		zbias_back = zbias;
	}


	// ESP_LOGI(FNAME,"Max X: %d Y: %d Z: %d  Min P: %d %d %d", xm, ym, zm, xi, yi, zi );

	if( !print ){
		const uint16_t X = 180;
		const uint16_t Y = 155;

		int16_t xp = int16_t(x*160/32768);
		int16_t yp = int16_t(y*160/32768);
		int16_t zp = int16_t(z*114/32768);

		xm = xm > xp ? xm : xp;
		ym = ym > yp ? ym : yp;
		zm = zm > zp ? zm : zp;
		xi = xi < xp ? xi : xp;
		yi = yi < yp ? yi : yp;
		zi = zi < zp ? zi : zp;
		if( b.ymax_green )
			menuPtr->ucg->setColor( COLOR_GREEN );
		else
			menuPtr->ucg->setColor( COLOR_RED );
		menuPtr->ucg->drawLine( X, Y, X, Y+ym );

		if( b.ymin_green )
			menuPtr->ucg->setColor( COLOR_GREEN );
		else
			menuPtr->ucg->setColor( COLOR_RED );
		menuPtr->ucg->drawLine( X, Y, X, Y+yi );

		if( b.xmax_green )
			menuPtr->ucg->setColor( COLOR_GREEN );
		else
			menuPtr->ucg->setColor( COLOR_RED );
		menuPtr->ucg->drawLine( X, Y, X+xm, Y );

		if( b.xmin_green )
			menuPtr->ucg->setColor( COLOR_GREEN );
		else
			menuPtr->ucg->setColor( COLOR_RED );
		menuPtr->ucg->drawLine( X, Y, X+xi, Y );

		if( b.zmax_green )
			menuPtr->ucg->setColor( COLOR_GREEN );
		else
			menuPtr->ucg->setColor( COLOR_RED );
		menuPtr->ucg->drawLine( X, Y, X+zm, Y-zm );    // 45 degree

		if( b.zmin_green )
			menuPtr->ucg->setColor( COLOR_GREEN );
		else
			menuPtr->ucg->setColor( COLOR_RED );
		menuPtr->ucg->drawLine( X, Y, X+zi, Y-zi );    // 45 degree

		static uint16_t x_old, y_old, z_old = 0;
		menuPtr->ucg->setColor( COLOR_BLACK );
		menuPtr->ucg->drawCircle( X, Y+y_old, 2 );
		menuPtr->ucg->drawCircle( X+x_old, Y, 2 );
		menuPtr->ucg->drawCircle( X+z_old, Y-z_old,2 );
		menuPtr->ucg->setColor( COLOR_WHITE );
		menuPtr->ucg->drawCircle( X, Y+yp, 2 );
		menuPtr->ucg->drawCircle( X+xp, Y, 2);
		menuPtr->ucg->drawCircle( X+zp, Y-zp, 2 );
		y_old = yp;
		x_old = xp;
		z_old = zp;
		bits_old = b;

	}


	xSemaphoreGive(spiMutex);
	// Stop further reporting.
	return true;
}
