/*
 * IpsDisplay.cpp
 *
 *  Created on: Oct 7, 2019
 *      Author: iltis
 *
 */


#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include <Ucglib.h>
#include "IpsDisplay.h"

int   IpsDisplay::tick = 0;
bool  IpsDisplay::_menu = false;
int   IpsDisplay::_pixpmd = 10;

#define DISPLAY_H 320
#define DISPLAY_W 240

// u8g2_t IpsDisplay::u8g2c;

const int   pixpm = 24; // pixel per meter
const int   dmid = 160;  // display middle
const int   bwide = 64; // total width of bargraph
const int   smfh  = 12;  // small font heigth
const int   hbw   = 12;  // horizontal bar width for unit of bargraph
const int   bw    = 44; // bar width
const int   trisize = 56; // triangle size quality up/down
#define abs(x)  (x < 0.0 ? -x : x)

Ucglib_ILI9341_18x240x320_HWSPI *IpsDisplay::ucg = 0;

IpsDisplay::IpsDisplay( Ucglib_ILI9341_18x240x320_HWSPI *aucg ) {
    ucg = aucg;
	_setup = 0;
	_dtype = ILI9341;
	_divisons = 5;
	_range_clip = 0;
	_range = 5;
	_clipte = 5;
	tick = 0;
	_dc = GPIO_NUM_MAX;
	_reset = GPIO_NUM_MAX;
	_cs = GPIO_NUM_MAX;
}

IpsDisplay::~IpsDisplay() {
}

float IpsDisplay::_range_clip = 0;
int   IpsDisplay::_divisons = 5;
Setup *IpsDisplay::_setup = 0;
float IpsDisplay::_range = 5;

void IpsDisplay::initDisplay() {
	setup();
	// ucg->clearScreen();
	// ucg->setFont(ucg_font_fub35_hn);
	ucg->setColor(255, 255, 255);
	ucg->drawBox( 0,0,240,320 );

	ucg->setColor(1, 255, 255, 255);
	ucg->setColor(0, 0, 0, 0);
	ucg->setFont(ucg_font_ncenR10_hr);
	ucg->setPrintPos(130,20);
	ucg->print("Vario");
	ucg->setPrintPos(210,42);
	ucg->print("m");
	ucg->setPrintPos(210,55);
	ucg->print("--");
	ucg->setPrintPos(210,65);
	ucg->print("s");

	// print TE scale
	for( int i=_divisons; i >=-_divisons; i-- )
	{
		float legend = ((float)i/(float)_divisons)*_range;  // only print the integers
		int hc = ucg->getFontAscent()/2;
		ucg->setPrintPos(5, dmid +hc - (legend*_pixpmd) );
		if( abs( legend  - int( legend )) < 0.01 )
			ucg->printf("%+d",(int)legend);
	}
	// Sollfahrt Text
	ucg->setPrintPos(130,100);
	ucg->print("Sollfahrt");
	ucg->setPrintPos(210,122);
	ucg->print("km");
	ucg->setPrintPos(210,135);
	ucg->print("--");
	ucg->setPrintPos(210,148);
	ucg->print("	h");

}

void IpsDisplay::begin( Setup* asetup ) {
	// printf("IpsDisplay::begin\n");
	ucg->begin(UCG_FONT_MODE_SOLID);
	_setup = asetup;
	setup();
}

void IpsDisplay::setup()
{
	_range = _setup->get()->_range;
	if( (int)_range %2 == 0 )
		_divisons = 4;
	else
		_divisons = 5;
	_pixpmd = (int)(( DISPLAY_H/2 -40) /_range);
	printf("Pixel per m/s %d", _pixpmd );
	_range_clip = _range*1.1;
}


int _te=0;
int yalt;
int s2falt=0;
extern xSemaphoreHandle spiMutex;

void IpsDisplay::drawDisplay( float te, float ate, float tealt, float temp, float volt, float s2fd, float s2f, float acl, bool s2fmode ){
	if( _menu )
			return;
	// printf("IpsDisplay::drawDisplay\n");
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ucg->setFont(ucg_font_fub35_hr);
	// ucg->setColor(1, 255, 255, 255);
	// ucg->setColor(0, 0, 0, 0);
	ucg->setColor( 0,0,0 );

	if( int(_te*30) != (int)(te*30) ) {
		ucg->setPrintPos(90,70);
		/*
		if( te < 0 ) {
			// xSemaphoreTake(spiMutex,portMAX_DELAY );
			ucg->print("-");
			// xSemaphoreGive(spiMutex);
		}
		else {
			// xSemaphoreTake(spiMutex,portMAX_DELAY );
			ucg->print("+");
			// xSemaphoreGive(spiMutex);
		}
		*/
		ucg->setPrintPos(80,70);
		// xSemaphoreTake(spiMutex,portMAX_DELAY );
		if( te > 0 )
			ucg->printf(" %0.1f ", abs(te) );
		else
			ucg->printf("-%0.1f ", abs(te) );
		// xSemaphoreGive(spiMutex);
		_te = te;
	}

	// printf("IpsDisplay setTE( %f %f )\n", te, ate);
	float _clipte = te;
	if ( te > _range_clip )
		_clipte = _range_clip;
	if ( te < -_range_clip )
		_clipte = -_range_clip;

	// Zero Line fat
	ucg->drawHLine( 0, dmid, 120 );
	ucg->drawHLine( 0, dmid-1, 120 );
	ucg->drawHLine( 0, dmid+1, 120 );

	// S2F
	int _s2f = (int)(s2f+0.5);
	if( _s2f != s2falt ) {
		ucg->setPrintPos(120,140);
		ucg->setFont(ucg_font_fub25_hn);
		// xSemaphoreTake(spiMutex,portMAX_DELAY );
		ucg->printf("%3d ", (int)(s2f+0.5)  );
		// xSemaphoreGive(spiMutex);
		s2falt = _s2f;
	}

/*
    // S2F Delta
	if( abs (s2fd) > 10 ) {
		if( s2fd < 0 ) {
			sprintf( buf,"%d", (int)(s2fd+0.5) );
			u8g2_DrawStr(&u8g2, dmid-9,32,buf);
		}else {
			sprintf( buf,"+%d", (int)(s2fd+0.5) );
			u8g2_DrawStr(&u8g2, dmid+1,32,buf);
		}
	}
	// TE alt
	u8g2_SetFont(&u8g2, u8g2_font_5x7_tr );
	sprintf( buf,"Alt");
	u8g2_DrawStr(&u8g2, 28,32,buf);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr );
	sprintf( buf,"%d m", (int)tealt );
	u8g2_DrawStr(&u8g2, 18,32,buf);

	tick++;
	if( tick < 50 ) {
	// Temperature headline, val
	u8g2_SetFont(&u8g2, u8g2_font_5x7_tr );
	sprintf( buf,"Temp C");
	u8g2_DrawStr(&u8g2, 10,26,buf);

	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tf );
	sprintf( buf,"%0.1f\xb0", temp );
	u8g2_DrawStr(&u8g2, 1,32,buf);
	}
	else if( tick < 100 ){
	// Battery headline, val
	u8g2_SetFont(&u8g2, u8g2_font_5x7_tr );
	sprintf( buf,"Battery");
	u8g2_DrawStr(&u8g2, 10,26,buf);
	sprintf( buf,"%0.1f V",_battery);
	u8g2_DrawStr(&u8g2, 1,32,buf);

	}
	else if( tick < 150 ){
		// Average climb headline, val
		u8g2_SetFont(&u8g2, u8g2_font_5x7_tr );
		sprintf( buf,"Climb MC");
		u8g2_DrawStr(&u8g2, 10,26,buf);
		sprintf( buf,"%0.1f m/s",acl);
		u8g2_DrawStr(&u8g2, 1,32,buf);
	}
	else
		tick = 0;

	u8g2_SetFont(&u8g2, u8g2_font_5x7_tr );
	u8g2_SetDrawColor( &u8g2, 2);

	*/
	// Now we draw the current TE value bar
	int y = int(_clipte*_pixpmd+0.5);
	// printf("Y=%d te=%f ppm%d\n", y, _clipte, _pixpmd );
 	if( y != yalt ) {
		// if TE value has changed
		int y = int(_clipte*_pixpmd+0.5);
		// we draw here the positive TE value bar
		if ( y > 0 ) {
		  ucg->setColor( 127,0,255 );
		  if( y > yalt ) {
			// xSemaphoreTake(spiMutex,portMAX_DELAY );
			ucg->drawBox( 40, dmid-y, bw, abs(y-yalt) );
			// xSemaphoreGive(spiMutex);
		  }
		  if( y < yalt  )
		  {
			ucg->setColor( 255,255,255 );
			// xSemaphoreTake(spiMutex,portMAX_DELAY );
			ucg->drawBox( 40, dmid-yalt, bw, abs(yalt-y) );
			// xSemaphoreGive(spiMutex);
			ucg->setColor( 0,0,0 );
		  }
		  if( yalt < 0 )
		  {
			ucg->setColor( 255,255,255 );
			// xSemaphoreTake(spiMutex,portMAX_DELAY );
			ucg->drawBox( 40, dmid, bw, abs(yalt) );
			// xSemaphoreGive(spiMutex);
			ucg->setColor( 0,0,0 );
		  }
		}
		// and now the negative TE value bar
		else  // y < 0
		{   // we have a bigger negative value so fill the delta
			ucg->setColor( 255-238,255-197,255-145 );
			if( y < yalt ) {
			   // xSemaphoreTake(spiMutex,portMAX_DELAY );
			   ucg->drawBox( 40, dmid-yalt, bw, abs(yalt-y) );
			   // xSemaphoreGive(spiMutex);
			}
			if( y > yalt )
			{  // a smaller negative value, blank the delta
				ucg->setColor( 255,255,255 );
				// xSemaphoreTake(spiMutex,portMAX_DELAY );
				ucg->drawBox( 40, dmid-y, bw, abs(yalt-y) );
				// xSemaphoreGive(spiMutex);
				ucg->setColor( 0,0,0 );
			}
			if( yalt > 0 )
			{  // blank the overshoot across zero
				ucg->setColor( 255,255,255 );
				// xSemaphoreTake(spiMutex,portMAX_DELAY );
				ucg->drawBox( 40, dmid-yalt, bw, yalt );
				// xSemaphoreGive(spiMutex);
				ucg->setColor( 0,0,0 );
			}

		}
	    yalt = y;
	}

	/*
	u8g2_DrawHLine( &u8g2, 0, 0, 127 );
	u8g2_DrawHLine( &u8g2, 0, bw, 127 );



	// S2F command trend bar and triangle
    int tri_len = int((trisize/2)* s2fd/20.0 );
    if( tri_len > trisize )
		tri_len = trisize;
    else if( tri_len < -trisize )
    	tri_len = -trisize;
    int box_len = 0;
    if( tri_len > 8 ) {
    	box_len = tri_len - 8;
        tri_len = 8;
    }
    else if( tri_len < -8 ) {
        box_len = tri_len + 8;
        tri_len = -8;
    }
    int tri_head = dmid - tri_len;
    if( tri_len > 0 ) {
    	u8g2_DrawBox(&u8g2, dmid-box_len,bw+9, box_len, trisize   );
    	u8g2_DrawTriangle(&u8g2, dmid-box_len,bw+9,
					         dmid-box_len,bw+9+trisize,
							 tri_head-box_len,bw+9+trisize/2 );
    }
    else    {  // negative values
    	u8g2_DrawBox(&u8g2, dmid,bw+9, -box_len, trisize   );
    	u8g2_DrawTriangle(&u8g2, dmid-box_len,bw+9,
    						         dmid-box_len,bw+9+trisize,
    								 tri_head-box_len,bw+9+trisize/2 );
    }

    // Small triangle pointing to Vario or S2F
    if( s2fmode )
    		u8g2_DrawTriangle(&u8g2, dmid-(int)(tri_len+box_len),bw+8,
    			dmid-(int)(tri_len+box_len)+8,bw,
    			dmid-(int)(tri_len+box_len)-8,bw );
    	else
    		u8g2_DrawTriangle(&u8g2, dmid+(int)(te*_pixpmd+0.5),bw,
    					dmid+(int)(te*_pixpmd+0.5)+8,bw+8,
    					dmid+(int)(te*_pixpmd+0.5)-8,bw+8 );


	u8g2_SendBuffer(&u8g2);
	*/

 	xSemaphoreGive(spiMutex);
}


