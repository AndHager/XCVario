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

const int   dmid = 160;   // display middle
const int   bwide = 64;   // total width of bargraph
const int   smfh  = 12;   // small font heigth
const int   hbw   = 12;   // horizontal bar width for unit of bargraph
const int   bw    = 32;   // bar width
const int   trisize = 120; // triangle size quality up/down

#define DISPLAY_LEFT 20

#define TRISIZE 15
#define abs(x)  (x < 0.0 ? -x : x)

#define FIELD_START 80
#define SIGNLEN 24+4
#define GAP 6

#define HEADFONTH 16
#define VARFONTH  42  // fub35_hn
#define YVAR HEADFONTH+VARFONTH
#define YVARMID (YVAR - (VARFONTH/2))

#define S2FFONTH 31
#define YS2F YVAR+S2FFONTH+HEADFONTH+GAP

#define VARBARGAP HEADFONTH
#define MAXS2FTRI 50

#define YALT YS2F+S2FFONTH+HEADFONTH+GAP+2*MAXS2FTRI

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






// draw all that does not need refresh when values change
void IpsDisplay::initDisplay() {
	setup();
	ucg->setColor(255, 255, 255);
	ucg->drawBox( 0,0,240,320 );

	ucg->setColor(1, 255, 255, 255);
	ucg->setColor(0, 0, 0, 0);
	ucg->setFont(ucg_font_fub11_tr);
	ucg->setPrintPos(0,YVAR-VARFONTH);
	ucg->print("Var m/s");
	ucg->setPrintPos(FIELD_START,YVAR-VARFONTH);    // 65 -52 = 13
	ucg->print("Average Vario");

	// print TE scale
	ucg->setFont(ucg_font_9x15B_mf);
	for( int i=_divisons; i >=-_divisons; i-- )
	{
		float legend = ((float)i*_range)/_divisons;  // only print the integers
		int hc = ucg->getFontAscent()/2;
		int y = dmid - (legend*_pixpmd);

		if( abs( legend  - int( legend )) < 0.1 ) {
			ucg->setPrintPos(0, y+hc  );
			ucg->printf("%+d",(int)legend);
		}
		ucg->drawHLine( DISPLAY_LEFT, y , 4 );
	}
	ucg->drawVLine( DISPLAY_LEFT+5,      VARBARGAP , DISPLAY_H-(VARBARGAP*2) );
	ucg->drawVLine( DISPLAY_LEFT+5+bw+1, VARBARGAP, DISPLAY_H-(VARBARGAP*2) );

	// Sollfahrt Text
	ucg->setFont(ucg_font_fub11_tr);
	ucg->setPrintPos(FIELD_START,YS2F-S2FFONTH);
	ucg->print("Speed2Fly");
	ucg->setFont(ucg_font_fub11_hr);
	int mslen = ucg->getStrWidth("km/h");
	ucg->setPrintPos(DISPLAY_W-mslen,YS2F);
	ucg->print("km/h");

	// Altitude
	ucg->setFont(ucg_font_fub11_tr);
	ucg->setPrintPos(FIELD_START,YALT-S2FFONTH);
	ucg->printf("Altitude QNH %d", (int)(_setup->get()->_QNH +0.5 ) );

	mslen = ucg->getStrWidth("m");
	ucg->setPrintPos(DISPLAY_W-mslen,YALT);
	ucg->print("m");

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

	if( (int)_range <= 5 )
		_divisons = (int)_range*2;
	else if( (int)_range == 6 )
		_divisons = 6;
	else if( (int)_range == 7 )
		_divisons = 7;
	else if( (int)_range == 8 )
		_divisons = 4;
	else if( (int)_range == 9 )
		_divisons = 3;
	else if( (int)_range == 10 )
		_divisons = 5;
	else if( (int)_range == 12 )
			_divisons = 6;
	else if( (int)_range == 13 )
			_divisons = 5;
	else if( (int)_range == 14 )
			_divisons = 7;
	else if( (int)_range == 15 )
				_divisons = 3;
	else
		_divisons = 5;

	_pixpmd = (int)(((DISPLAY_H-(2*VARBARGAP))/2) /_range);
	printf("Pixel per m/s %d", _pixpmd );
	_range_clip = _range*1.1;


}


int _te=0;
int yalt;
int s2falt=0;
int s2fdalt=0;
int prefalt=0;
int tevlalt=0;

extern xSemaphoreHandle spiMutex;

#define PMLEN 24

void IpsDisplay::drawDisplay( float te, float ate, float altitude, float temp, float volt, float s2fd, float s2f, float acl, bool s2fmode ){
	if( _menu )
			return;
	// printf("IpsDisplay::drawDisplay\n");
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ucg->setFont(ucg_font_fub35_hn);  // 52 height
	ucg->setColor( 0,0,0 );
	// ucg->setColor(1, 127, 127, 127);
	if( int(_te*10) != (int)(te*10) ) {
		if( te < 0 ) {
			// erase V line from +
			ucg->setColor( 255,255,255 );
			ucg->drawVLine( FIELD_START+PMLEN/2-1, YVARMID-PMLEN/2, PMLEN );
			ucg->drawVLine( FIELD_START+PMLEN/2, YVARMID-PMLEN/2, PMLEN );
			ucg->drawVLine( FIELD_START+PMLEN/2+1, YVARMID-PMLEN/2, PMLEN );
			// draw just minus
			ucg->setColor( 0,0,0 );
			ucg->drawHLine( FIELD_START, YVARMID-1, PMLEN );
			ucg->drawHLine( FIELD_START, YVARMID, PMLEN );
			ucg->drawHLine( FIELD_START, YVARMID+1, PMLEN );

		}
		else {
			// draw just plus
			ucg->drawHLine( FIELD_START, YVARMID-1, PMLEN );
			ucg->drawHLine( FIELD_START, YVARMID, PMLEN );
			ucg->drawHLine( FIELD_START, YVARMID+1, PMLEN );
			ucg->drawVLine( FIELD_START+PMLEN/2-1, YVARMID-PMLEN/2, PMLEN );
			ucg->drawVLine( FIELD_START+PMLEN/2, YVARMID-PMLEN/2, PMLEN );
			ucg->drawVLine( FIELD_START+PMLEN/2+1, YVARMID-PMLEN/2, PMLEN );
		}
		char tev[10];
		sprintf( tev, "%0.1f  ", abs(te) );
		ucg->setPrintPos(FIELD_START+SIGNLEN,YVAR);
		ucg->print(tev);
		ucg->setFont(ucg_font_fub11_hr);
		int mslen = ucg->getStrWidth(" m/s");
		ucg->setPrintPos(DISPLAY_W-mslen,YVAR);
		ucg->print(" m/s");
		_te = te;
	}

	// printf("IpsDisplay setTE( %f %f )\n", te, ate);
	float _clipte = te;
	if ( te > _range_clip )
		_clipte = _range_clip;
	if ( te < -_range_clip )
		_clipte = -_range_clip;

	// Zero Line fat
	// ucg->drawHLine( 40, dmid, 120 );
	// ucg->drawHLine( 40, dmid-1, 120 );
	// ucg->drawHLine( 40, dmid+1, 120 );

	int y = int(_clipte*_pixpmd+0.5);

	/*   Zeiger Idee erstmal auf Eis, klappt so einfach nicht
	if( y != yalt ) {
	// Zeiger clear
		ucg->setColor( 255,255,255 );
		for( int i=-2; i<=2; i++ )
			ucg->drawLine( 30+abs(i), dmid-yalt+i, 90, dmid+i );
		// Zeiger draw
		ucg->setColor( 0, 0, 0 );
		for( int i=-2; i<=2; i++ )
			ucg->drawLine( 30+abs(i), dmid-y+i, 90, dmid+i );
	}
	*/

	// ucg->drawLine( 40, yalt, dmid-1, 120 );
	// ucg->drawLine( 40, yalt, dmid+1, 120 );

	// S2F
	int _s2f = (int)(s2f+0.5);
	if( _s2f != s2falt ) {
		ucg->setPrintPos(FIELD_START,YS2F);
		ucg->setFont(ucg_font_fub25_hn);
		ucg->printf("  %3d ", (int)(s2f+0.5)  );
		s2falt = _s2f;

	}

	// S2F
	int alt = (int)(altitude+0.5);
	if( alt != prefalt ) {
		ucg->setPrintPos(FIELD_START,YALT);
		ucg->setFont(ucg_font_fub25_hn);
		ucg->printf(" %4d ", (int)(alt+0.5)  );
		prefalt = alt;
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

	// Temperature headline, val
	u8g2_SetFont(&u8g2, u8g2_font_5x7_tr );
	sprintf( buf,"Temp C");
	u8g2_DrawStr(&u8g2, 10,26,buf);

	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tf );
	sprintf( buf,"%0.1f\xb0", temp );
	u8g2_DrawStr(&u8g2, 1,32,buf);

	// Battery headline, val
	u8g2_SetFont(&u8g2, u8g2_font_5x7_tr );
	sprintf( buf,"Battery");
	u8g2_DrawStr(&u8g2, 10,26,buf);
	sprintf( buf,"%0.1f V",_battery);
	u8g2_DrawStr(&u8g2, 1,32,buf);
		// Average climb headline, val
		u8g2_SetFont(&u8g2, u8g2_font_5x7_tr );
		sprintf( buf,"Climb MC");
		u8g2_DrawStr(&u8g2, 10,26,buf);
		sprintf( buf,"%0.1f m/s",acl);
		u8g2_DrawStr(&u8g2, 1,32,buf);

	else
		tick = 0;

	u8g2_SetFont(&u8g2, u8g2_font_5x7_tr );
	u8g2_SetDrawColor( &u8g2, 2);

	*/
	// Now we draw the current TE value bar

	// printf("Y=%d te=%f ppm%d\n", y, _clipte, _pixpmd );

 	if( y != yalt ) {
		// if TE value has changed
		int y = int(_clipte*_pixpmd+0.5);
		// we draw here the positive TE value bar
		if ( y > 0 ) {
		  ucg->setColor( 127,0,255 );
		  if( y > yalt ) {
			// xSemaphoreTake(spiMutex,portMAX_DELAY );
			ucg->drawBox( DISPLAY_LEFT+6, dmid-y, bw, abs(y-yalt) );
			// xSemaphoreGive(spiMutex);
		  }
		  if( y < yalt  )
		  {
			ucg->setColor( 255,255,255 );
			// xSemaphoreTake(spiMutex,portMAX_DELAY );
			ucg->drawBox( DISPLAY_LEFT+6, dmid-yalt, bw, abs(yalt-y) );
			// xSemaphoreGive(spiMutex);
			ucg->setColor( 0,0,0 );
		  }
		  if( yalt < 0 )
		  {
			ucg->setColor( 255,255,255 );
			// xSemaphoreTake(spiMutex,portMAX_DELAY );
			ucg->drawBox( DISPLAY_LEFT+6, dmid, bw, abs(yalt) );
			// xSemaphoreGive(spiMutex);
			ucg->setColor( 0,0,0 );
		  }
		}
		// and now the negative TE value bar
		else  // y < 0
		{   // we have a bigger negative value so fill the delta
			ucg->setColor( 0, 180, 180 );
			if( y < yalt ) {
			   // xSemaphoreTake(spiMutex,portMAX_DELAY );
			   ucg->drawBox( DISPLAY_LEFT+6, dmid-yalt, bw, abs(yalt-y) );
			   // xSemaphoreGive(spiMutex);
			}
			if( y > yalt )
			{  // a smaller negative value, blank the delta
				ucg->setColor( 255,255,255 );
				// xSemaphoreTake(spiMutex,portMAX_DELAY );
				ucg->drawBox( DISPLAY_LEFT+6, dmid-y, bw, abs(yalt-y) );
				// xSemaphoreGive(spiMutex);
				ucg->setColor( 0,0,0 );
			}
			if( yalt > 0 )
			{  // blank the overshoot across zero
				ucg->setColor( 255,255,255 );
				// xSemaphoreTake(spiMutex,portMAX_DELAY );
				ucg->drawBox( DISPLAY_LEFT+6, dmid-yalt, bw, yalt );
				// xSemaphoreGive(spiMutex);
				ucg->setColor( 0,0,0 );
			}

		}
		// Small triangle pointing to actual value
		// First blank the old one
		ucg->setColor( 255,255,255 );
		ucg->drawTriangle( DISPLAY_LEFT+4+bw+3,         dmid-yalt,
						   DISPLAY_LEFT+4+bw+3+TRISIZE, dmid-yalt+TRISIZE,
						   DISPLAY_LEFT+4+bw+3+TRISIZE, dmid-yalt-TRISIZE );
		// Draw a new one at current position
		ucg->setColor( 0,0,0 );
		ucg->drawTriangle( DISPLAY_LEFT+4+bw+3,         dmid-y,
						   DISPLAY_LEFT+4+bw+3+TRISIZE, dmid-y+TRISIZE,
						   DISPLAY_LEFT+4+bw+3+TRISIZE, dmid-y-TRISIZE );

	    yalt = y;
	}

 	// S2F command trend triangle


 	if( (int)s2fd != s2fdalt ) {
 		ucg->setClipRange( FIELD_START, dmid-MAXS2FTRI, trisize, MAXS2FTRI*2 );
 		bool clear = false;
 		if( s2fd > 0 ) {
 			if ( (int)s2fd < s2fdalt || (int)s2fdalt < 0 )
 				clear = true;
 		}
 		else {
 			if ( (int)s2fd > s2fdalt || (int)s2fdalt > 0  )
 		 		clear = true;
 		}

 		if( clear ) {
			ucg->setColor( 255,255,255 );
			ucg->drawTriangle( FIELD_START, dmid,
							   FIELD_START+(trisize/2), dmid+(int)s2fd,
							   FIELD_START+(trisize/2), dmid+(int)s2fdalt );
			ucg->drawTriangle( FIELD_START+trisize, dmid,
							   FIELD_START+(trisize/2), dmid+(int)s2fd,
							   FIELD_START+(trisize/2), dmid+(int)s2fdalt );
 		}
 		else{
			ucg->setColor( 0,0,0 );
			ucg->drawTriangle( FIELD_START, dmid,
							   FIELD_START+trisize, dmid,
							   FIELD_START+(trisize/2), dmid+(int)s2fd );
 		}
 		ucg->undoClipRange();

 		s2fdalt = (int)s2fd;
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




	u8g2_SendBuffer(&u8g2);
	*/

 	xSemaphoreGive(spiMutex);
}


