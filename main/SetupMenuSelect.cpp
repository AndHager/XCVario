


/*
 * SetupMenu.cpp
 *
 *  Created on: Feb 4, 2018
 *      Author: iltis
 */

#include <logdef.h>
#include <sensor.h>
#include "Units.h"
#include "SetupMenuSelect.h"
#include "ESPAudio.h"


const char * SetupMenuSelect::getEntry() const
{
	ESP_LOGI(FNAME,"getEntry() select:%d", _select );
	return _values[ _select ];
}

const char *SetupMenuSelect::value() const {
	return getEntry();
}


bool SetupMenuSelect::existsEntry( std::string ent ){
	for( std::vector<const char*>::iterator iter = _values.begin(); iter != _values.end(); ++iter )
		if( std::string(*iter) == ent )
			return true;
	return false;
}

void SetupMenuSelect::addEntryList( const char ent[][4], int size )
{
	ESP_LOGI(FNAME,"SetupMenuSelect::addEntryList() char ent[][4]");
    for( int i=0; i<size; i++ ) {
    	// ESP_LOGI(FNAME,"add ent:%s  num:%d", std::string(ent[i]), _numval );
        _values.push_back( (char *)ent[i] ); _numval++;
    }
}

void SetupMenuSelect::delEntry( const char* ent ) {
	for( std::vector<const char *>::iterator iter = _values.begin(); iter != _values.end(); ++iter )
		if( std::string(*iter) == std::string(ent) )
		{
			_values.erase( iter );
			_numval--;
			if( _select >= _numval )
				_select = _numval-1;
			break;
		}
}

SetupMenuSelect::SetupMenuSelect( const char* title, bool restart, int (*action)(SetupMenuSelect *p), bool save, SetupNG<int> *anvs, bool end_menu ) {
	ESP_LOGI(FNAME,"SetupMenuSelect( %s ) action: %x", title, (int)action );
	_rotary->attach(this);
	_end_menu = end_menu;
	_title = title;
	_nvs = 0;
	_select = 0;
	_select_save = 0;
	highlight = -1;
	select_intern = 0;
	if( !anvs ) {
		_select = select_intern;
		_select_save = select_intern;
	}
	_numval = 0;
	_restart = restart;
	_action = action;
	_save = save;
	if( anvs ) {
		_nvs = anvs;
		ESP_LOGI(FNAME,"_nvs->key(): %s val: %d", _nvs->key(), (int)(_nvs->get()) );
		_select = (int16_t)(*(int *)(_nvs->getPtr()));
		_select_save = (int16_t)_nvs->get();
	}

}
SetupMenuSelect::~SetupMenuSelect()
{
    _rotary->detach(this);
}


void SetupMenuSelect::display( int mode ){
	if( (selected != this) || !inSetup )
		return;
	ESP_LOGI(FNAME,"display() pressed:%d title:%s action: %x", pressed, _title, (int)(_action) );
	clear();
	if( _action != 0 ){
		ESP_LOGI(FNAME,"calling action");
		(*_action)( this );
	}
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ucg->setPrintPos(1,25);
	ESP_LOGI(FNAME,"Title: %s y=%d", _title,y );
	ucg->printf("<< %s",_title);
	xSemaphoreGive(spiMutex );
	ESP_LOGI(FNAME,"select=%d numval=%d size=%d val=%s", _select, _numval, _values.size(), _values[_select] );
	if( _numval > 9 ){
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		ucg->setPrintPos( 1, 50 );
		ucg->printf( "%s                ", _values[_select] );
		xSemaphoreGive(spiMutex );
	}else
	{
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		for( int i=0; i<_numval && i<+10; i++ )	{
			ucg->setPrintPos( 1, 50+25*i );
			ucg->print( _values[i] );
		}
		ucg->drawFrame( 1,(_select+1)*25+3,238,25 );
		xSemaphoreGive(spiMutex );
	}

	y=_numval*25+50;
	showhelp( y );
	if(mode == 1 && _save == true ){
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		ucg->setPrintPos( 1, 300 );
		ucg->print("Saved" );
		xSemaphoreGive(spiMutex );
	}
	if( mode == 1 )
		delay(1000);
}

void SetupMenuSelect::down(int count){
	if( (selected != this) || !inSetup )
		return;

	if( _numval > 9 ){
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		while( count ) {
			if( (_select) > 0 )
				(_select)--;
			count--;
		}
		ucg->setPrintPos( 1, 50 );
		ucg->setFont(ucg_font_ncenR14_hr, true );
		ucg->printf("%s                  ",_values[_select] );
		xSemaphoreGive(spiMutex );
	}else {
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		ucg->setColor(COLOR_BLACK);
		ucg->drawFrame( 1,(_select+1)*25+3,238,25 );  // blank old frame
		ucg->setColor(COLOR_WHITE);
		if( (_select) >  0 )
			(_select)--;
		ESP_LOGI(FNAME,"val down %d", _select );
		ucg->drawFrame( 1,(_select+1)*25+3,238,25 );  // draw new frame
		xSemaphoreGive(spiMutex );
	}
}

void SetupMenuSelect::up(int count){
	if( (selected != this) || !inSetup )
		return;
	if( _numval > 9 )
	{
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		while( count ) {
			if( (_select) <  _numval-1 )
				(_select)++;
			count--;
		}
		ucg->setPrintPos( 1, 50 );
		ucg->setFont(ucg_font_ncenR14_hr, true );
		ucg->printf("%s                   ", _values[_select] );
		xSemaphoreGive(spiMutex );
	}else {
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		ucg->setColor(COLOR_BLACK);
		ucg->drawFrame( 1,(_select+1)*25+3,238,25 );  // blank old frame
		ucg->setColor(COLOR_WHITE);
		if ( (_select) < _numval-1 )
			(_select)++;
		ESP_LOGI(FNAME,"val up %d", _select );
		ucg->drawFrame( 1,(_select+1)*25+3,238,25 );  // draw new frame
		xSemaphoreGive(spiMutex );
	}
}

void SetupMenuSelect::longPress(){
	press();
}

void SetupMenuSelect::press(){
	if( selected != this )
		return;
	ESP_LOGI(FNAME,"SetupMenuSelect press");
	if ( pressed ){
		display( 1 );
		if( _parent != 0) {
			selected = _parent;
			_parent->highlight = -1;  // to topmost selection when back
		}
		selected->pressed = true;
		if( _nvs ){
			_nvs->set((int)_select, false ); // do sync in next step
			_nvs->commit();
		}
		pressed = false;
		if( _select_save != _select )
			if( _restart ) {
				Audio::shutdown();
				clear();
				ucg->setPrintPos( 10, 50 );
				ucg->print("...rebooting now" );
				SetupCommon::commitNow();
				delay(2000);
				esp_restart();
			}
	}
	else
	{
		pressed = true;
	}
}
