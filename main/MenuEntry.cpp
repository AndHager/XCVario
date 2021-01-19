/*
 * SetupMenu.cpp
 *
 *  Created on: Feb 4, 2018
 *      Author: iltis
 */

#include "SetupMenu.h"
#include "IpsDisplay.h"
#include <inttypes.h>
#include <iterator>
#include <algorithm>
#include "ESPAudio.h"
#include "BMPVario.h"
#include "S2F.h"
#include "Version.h"
#include "Polars.h"
#include <logdef.h>
#include <sensor.h>
#include "Cipher.h"
#include "Units.h"
#include "Switch.h"
#include "Flap.h"
#include "MenuEntry.h"

Ucglib_ILI9341_18x240x320_HWSPI *MenuEntry::ucg = 0;
IpsDisplay* MenuEntry::_display = 0;
MenuEntry* MenuEntry::root = 0;
MenuEntry* MenuEntry::selected = 0;
ESPRotary* MenuEntry::_rotary = 0;
AnalogInput* MenuEntry::_adc = 0;
BME280_ESP32_SPI *MenuEntry::_bmp = 0;
float MenuEntry::volume;
bool  MenuEntry::_menu_enabled = false;


void MenuEntry::uprintf( int x, int y, const char* format, ...) {
	if( ucg == 0 ) {
		ESP_LOGE(FNAME,"Error UCG not initialized !");
		return;
	}
	va_list argptr;
	va_start(argptr, format);
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ucg->setPrintPos(x,y);
	ucg->printf( format, argptr );
	xSemaphoreGive(spiMutex );
	va_end(argptr);
}

void MenuEntry::uprint( int x, int y, const char* str ) {
	if( ucg == 0 ) {
		ESP_LOGE(FNAME,"Error UCG not initialized !");
		return;
	}
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ucg->setPrintPos(x,y);
	ucg->print( str );
	xSemaphoreGive(spiMutex );
}

MenuEntry* MenuEntry::addMenu( MenuEntry * item ) {
	// ESP_LOGI(FNAME,"MenuEntry addMenu() title %s", item->_title.c_str() );
	if( root == 0 ){
		ESP_LOGI(FNAME,"Init root menu");
		root = item;
		item->_parent = 0;
		selected = item;
		return item;
	}
	else{
		// ESP_LOGI(FNAME,"add to childs");
		item->_parent = this;
		_childs.push_back( item );
		return item;
	}
}

void MenuEntry::delMenu( MenuEntry * item ) {
	ESP_LOGI(FNAME,"MenuEntry delMenu() title %s", item->_title.c_str() );
	std::vector<MenuEntry *>::iterator position = std::find(_childs.begin(), _childs.end(), item );
	if (position != _childs.end()) { // == myVector.end() means the element was not found
		ESP_LOGI(FNAME,"found entry, now erase" );
		_childs.erase(position);
	}
}

MenuEntry* MenuEntry::findMenu( String title, MenuEntry* start )
{
	ESP_LOGI(FNAME,"MenuEntry findMenu() %s %x", title.c_str(), (uint32_t)start );
	if( start->_title == title ) {
		ESP_LOGI(FNAME,"Menu entry found for start %s", title.c_str() );
		return start;
	}
	for(MenuEntry* child : start->_childs) {
		if( child->_title == title )
			return child;
		MenuEntry* m = child->findMenu( title, child );
		if( m != 0 ) {
			ESP_LOGI(FNAME,"Menu entry found for %s", title.c_str() );
			return m;
		}
	};
	ESP_LOGW(FNAME,"Menu entry not found for %s", title.c_str() );
	return 0;
}

void MenuEntry::showhelp( int y ){
	if( helptext != 0 ){
		int w=0;
		char buf[512];
		memset(buf, 0, 512);
		memcpy( buf, helptext, strlen(helptext));
		char *p = strtok (buf, " ");
		char *words[100];
		while (p != NULL)
		{
			words[w++] = p;
			p = strtok (NULL, " ");
		}
		// ESP_LOGI(FNAME,"showhelp number of words: %d", w);
		int x=1;
		int y=hypos;
		ucg->setFont(ucg_font_ncenR14_hr);
		for( int p=0; p<w; p++ )
		{
			int len = ucg->getStrWidth( words[p] );
			// ESP_LOGI(FNAME,"showhelp pix len word #%d = %d, %s ", p, len, words[p]);
			if( x+len > 239 ) {   // does still fit on line
				y+=25;
				x=1;
			}
			xSemaphoreTake(spiMutex,portMAX_DELAY );
			ucg->setPrintPos(x, y);
			ucg->print( words[p] );
			xSemaphoreGive(spiMutex );
			x+=len+5;
		}
	}
}

void MenuEntry::clear()
{
	ESP_LOGI(FNAME,"MenuEntry::clear");
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ucg->setColor(COLOR_BLACK);
	ucg->drawBox( 0,0,240,320 );
	// ucg->begin(UCG_FONT_MODE_SOLID);
	ucg->setFont(ucg_font_ncenR14_hr);
	ucg->setPrintPos( 1, 30 );
	ucg->setColor(COLOR_WHITE);
	xSemaphoreGive(spiMutex );
}

