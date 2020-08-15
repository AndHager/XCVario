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
#include "ESPAudio.h"
#include "BMPVario.h"
#include "S2F.h"
#include "Version.h"
#include "Polars.h"

IpsDisplay* MenuEntry::_display = 0;
MenuEntry* MenuEntry::root = 0;
MenuEntry* MenuEntry::selected = 0;
ESPRotary* MenuEntry::_rotary = 0;
Setup* MenuEntry::_setup = 0;
BatVoltage* MenuEntry::_adc = 0;
BME280_ESP32_SPI *MenuEntry::_bmp = 0;
float MenuEntry::volume;
bool      MenuEntry::_menu_enabled = false;
extern S2F s2f;
extern xSemaphoreHandle spiMutex;
Ucglib_ILI9341_18x240x320_HWSPI *MenuEntry::ucg = 0;
static char rentry[25];
SetupMenuSelect * audio_range_sm = 0;


int update_rentry(SetupMenuValFloat * p)
{
	char rentry[25];
    sprintf( rentry, "Variable (%d m/s)", (int)(p->_setup->get()->_range) );
    audio_range_sm->updateEntry( rentry, 1 );
    return 0;
}

// Action Routines
int contrast( SetupMenuValFloat * p )
{
	printf("contrast( %f ) \n", *p->_value );
	return 0;
}

int qnh_adj( SetupMenuValFloat * p )
{
	// printf("qnh_adj");
	float alt=0;
	for( int i=0; i<6; i++ ) {
		alt += p->_bmp->readAltitude( *(p->_value) );
		sleep(0.01);
	}
	alt = alt/6;

	// printf("Setup BA alt=%f QNH=%f\n", alt, *(p->_value)  );
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	p->ucg->setFont(ucg_font_fub25_hr);
	p->ucg->setPrintPos(1,120);
	String u;
	float altp;
	if( p->_setup->get()->_alt_unit == 0 ){ // m
		u = "m";
	    altp = alt;
	}
	else {
		u = "ft";
		altp = alt*3.28084;
	}

	p->ucg->printf("%4d %s ", (int)(altp+0.5), u.c_str() );
	p->ucg->setFont(ucg_font_ncenR14_hr);
	xSemaphoreGive(spiMutex );
	return 0;
}

int elev_adj( SetupMenuValFloat * p )
{
	// printf("elev_adj");
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	p->ucg->setFont(ucg_font_fub25_hr);
	p->ucg->setPrintPos(1,120);
	String u;
	float elevp = p->_setup->get()->_elevation;
	if( p->_setup->get()->_alt_unit == 0 ){ // m
		u = "m";
	}
	else {
		u = "ft";
		elevp = elevp*3.28084;
	}
	p->ucg->printf("%4d %s ", (int)(elevp+0.5), u.c_str() );
	p->ucg->setFont(ucg_font_ncenR14_hr);
	xSemaphoreGive(spiMutex );
	return 0;
}


// Battery Voltage Meter Calibration
int factv_adj( SetupMenuValFloat * p )
{
	printf("factv_adj");
	float bat = p->_adc->getBatVoltage(true);
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	p->ucg->setPrintPos(1,100);
	p->ucg->printf("%0.2f Volt", bat);
	xSemaphoreGive(spiMutex );
	return 0;
}


int polar_adj( SetupMenuValFloat * p )
{
    s2f.change_polar();
    return 0;
}

int polar_select( SetupMenuSelect * p )
{
    s2f.select_polar();
    return 0;
}

int bal_adj( SetupMenuValFloat * p )
{
    s2f.change_mc_bal();
    float loadinc = (p->_setup->get()->_ballast +100.0)/100.0;
    float newwl = p->_setup->get()->_polar.wingload * loadinc;
    p->ucg->setFont(ucg_font_fub25_hr);
    xSemaphoreTake(spiMutex,portMAX_DELAY );
    p->ucg->setPrintPos(1,110);
    p->ucg->printf("%0.2f kg/m2", newwl);
	xSemaphoreGive(spiMutex );
	p->ucg->setFont(ucg_font_ncenR14_hr);
    return 0;
}

int bug_adj( SetupMenuValFloat * p ){
	 s2f.change_mc_bal();
	 return 0;
}


int mc_adj( SetupMenuValFloat * p )
{
    s2f.change_mc_bal();
#ifdef MC_FROM_XC
    char mc[30];
    sprintf(mc, "$POV,?,MC");
    // sprintf(mc, "$POV,C,MC,%0.2f", p->_setup->get()->_MC );
	int cs = OpenVario::getCheckSum(&mc[1]);
	int i = strlen(mc);
	sprintf( &mc[i], "*%02X\n", cs );
    btsender.send(mc);
#endif
    return 0;
}

int vol_adj( SetupMenuValFloat * p ){
	Audio.setVolume( (int(p->volume*1.270)));
	return 0;
}

SetupMenu::SetupMenu(){
	highlight = -1;
	_parent = 0;
	y = 0;
	helptext = 0;
	long_pressed = false;
}

SetupMenu::SetupMenu( String title ) {
	// printf("SetupMenu::SetupMenu( %s ) \n", title.c_str() );
	_rotary->attach(this);
	_title = title;
	highlight = -1;
}

void SetupMenu::begin( IpsDisplay* display, ESPRotary * rotary, Setup* my_setup, BME280_ESP32_SPI * bmp, BatVoltage *adc ){
	printf("SetupMenu() begin\n");
	_rotary = rotary;
	_setup = my_setup;
	_bmp = bmp;
	_display = display;
	ucg = display->getDisplay();
	_adc = adc;
	setup();
	volume = _setup->get()->_default_volume;
}

void MenuEntry::uprintf( int x, int y, const char* format, ...) {
	if( ucg == 0 ) {
		printf("Error UCG not initialized !");
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
		printf("Error UCG not initialized !");
		return;
	}
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ucg->setPrintPos(x,y);
	ucg->print( str );
	xSemaphoreGive(spiMutex );
}


MenuEntry* MenuEntry::addMenu( MenuEntry * item ) {
	// printf("MenuEntry addMenu() title %s\n", item->_title.c_str() );
	if( root == 0 ){
		printf("Init root menu\n");
		root = item;
		item->_parent = 0;
		selected = item;
		return item;
	}
	else{
		// printf("add to childs\n");
		item->_parent = this;
		_childs.push_back( item );
		return item;
	}
}

MenuEntry* MenuEntry::findMenu( String title, MenuEntry* start )
{
	printf("MenuEntry findMenu() %s %x\n", title.c_str(), (uint32_t)start );
	if( start->_title == title ) {
		printf("Menu entry found for start %s\n", title.c_str() );
		return start;
	}
	for(MenuEntry* child : start->_childs) {
		if( child->_title == title )
			return child;
		MenuEntry* m = child->findMenu( title, child );
		if( m != 0 ) {
			printf("Menu entry found for %s\n", title.c_str() );
			return m;
		}
	};
	printf("Menu entry not found for %s\n", title.c_str() );
	return 0;
}


void SetupMenu::display( int mode ){
	if( (selected != this) || !_menu_enabled )
		return;
	printf("SetupMenu display( %s)\n", _title.c_str() );
	clear();
	y=25;
	printf("Title: %s y=%d\n", selected->_title.c_str(),y );
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ucg->setPrintPos(1,y);
	ucg->printf("<< %s",selected->_title.c_str());
	ucg->drawFrame( 1,3,238,25 );

	for (int i=0; i<_childs.size(); i++ ) {
		MenuEntry * child = _childs[i];
		ucg->setPrintPos(1,(i+1)*25+25);
		ucg->printf("%s",child->_title.c_str());
		printf("Child: %s y=%d\n",child->_title.c_str() ,y );
	}
	y+=170;
	xSemaphoreGive(spiMutex );
	showhelp( y );
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
		// printf("showhelp number of words: %d\n", w);
		int x=1;
		int y=hypos;
		ucg->setFont(ucg_font_ncenR14_hr);
		for( int p=0; p<w; p++ )
		{
			int len = ucg->getStrWidth( words[p] );
			// printf("showhelp pix len word #%d = %d, %s \n", p, len, words[p]);
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

void dec_volume( int count ) {
	Audio.decVolume(count);
}

void inc_volume( int count ) {
	Audio.incVolume(count);
}


void SetupMenu::down(int count){
	if( selected == this && !_menu_enabled ) {
		printf("root: down\n");
		if(_setup->get()->_rot_default == 1) {	 // MC Value
			if( _setup->get()->_MC > 0.1 ) {
				_setup->get()->_MC -= 0.1;
				s2f.change_mc_bal();
			}
		}
		else
			inc_volume( count );
	}
	if( (selected != this) || !_menu_enabled )
		return;
	printf("down %d %d\n", highlight, _childs.size() );
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ucg->setColor(COLOR_BLACK);
	ucg->drawFrame( 1,(highlight+1)*25+3,238,25 );
	ucg->setColor(COLOR_WHITE);
	if( highlight  > -1 ){
		highlight --;
	}
	else
		highlight = (int)(_childs.size() -1 );
	ucg->drawFrame( 1,(highlight+1)*25+3,238,25 );
	xSemaphoreGive(spiMutex );
	pressed = true;
}

void SetupMenu::up(int count){
	if( selected == this && !_menu_enabled ) {
		printf("root: up\n");
		if(_setup->get()->_rot_default == 1) {	 // MC Value
			if( _setup->get()->_MC < 9.9 ) {
				_setup->get()->_MC += 0.1;
				s2f.change_mc_bal();
			}
		}
		else
			dec_volume( count );
	}

	if( (selected != this) || !_menu_enabled )
		return;
	printf("SetupMenu::up %d %d\n", highlight, _childs.size() );
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ucg->setColor(COLOR_BLACK);
	ucg->drawFrame( 1,(highlight+1)*25+3,238,25 );
	ucg->setColor(COLOR_WHITE);
	if( highlight < (int)(_childs.size()-1) ){
		highlight ++;
	}
	else
		highlight = -1;
	ucg->drawFrame( 1,(highlight+1)*25+3,238,25 );
	pressed = true;
	xSemaphoreGive(spiMutex );
}

void SetupMenu::longPress(){
	long_pressed = true;
	printf("SetupMenue: longPress\n");
	// Audio.disable();
	// delay(1000);
	// esp_restart();
}


void SetupMenu::press(){
	if( selected == 0 )
		selected = root;
	if( (selected != this) )
		return;
	printf("SetupMenu press p:%d h:%d parent:%x\n", pressed, highlight, (int)_parent );
	// main menue behavior, extra class maybe
	// for now here we catch this
	// this is just for the main menu,
	if( (_parent == 0) && (highlight == -1) )
	{
		if( !pressed )
		{
			printf("!pressed\n");
			Audio.disable();
			delay( 500 );
			_menu_enabled = true;
		}
		else
		{
			printf("pressed\n");
			_display->setup();
			_display->initDisplay();
			Audio.setup();
			bmpVario.setup();
			_menu_enabled = false;
			Audio.disable(false);
		}
	}
	// default is not pressed, so just display, but we toogle pressed state at the end
	// so next time we either step up to parent,
	if( pressed )
	{
		if( highlight == -1 ) {
			printf("SetupMenu to parent\n");
			if( _parent != 0 ){
				selected = _parent;
				selected->highlight = -1;
				selected->pressed = true;
			}
		}
		else {
			printf("SetupMenu to child\n");
			if( (highlight >=0) && (highlight < (int)(_childs.size()) ) ){
				selected = _childs[highlight];
				selected->pressed = false;
			}
		}
		pressed = false;
	}
	else
	{
		pressed = true;
	}
	printf("~SetupMenu press()\n");
}




void SetupMenu::setup( )
{
	printf("SetupMenu setup()\n");
	SetupMenu * root = new SetupMenu( "Setup" );
	MenuEntry* mm = root->addMenu( root );

	SetupMenuValFloat * mc = new SetupMenuValFloat( "MC", &_setup->get()->_MC, "m/s",	0.01, 9.9, 0.1,  mc_adj, true );
	mc->setHelp(PROGMEM"Default Mac Cready value for optimum cruise speed, or average climb rate, MC is provided in usual metric system means");
	mm->addMenu( mc );

	SetupMenuValFloat * vol = new SetupMenuValFloat( "Audio Volume", &volume, "%", 0, 100, 1, vol_adj, true  );
	vol->setHelp(PROGMEM"Set audio volume");
	mm->addMenu( vol );

	SetupMenuValFloat::qnh_menu = new SetupMenuValFloat( "QNH Setup", &_setup->get()->_QNH, "hPa", 900.0, 1100.0, 0.250, qnh_adj, true );
	SetupMenuValFloat::qnh_menu->setHelp(PROGMEM"Setup QNH pressure value from next ATC. On ground you may adjust to airfield altitude above MSL.");
	mm->addMenu( SetupMenuValFloat::qnh_menu );

	SetupMenuValFloat * bal = new SetupMenuValFloat( "Ballast", &_setup->get()->_ballast, "%", 0.0, 100, 1, bal_adj, true  );
	bal->setHelp(PROGMEM"Percent wing load increase by ballast");
	mm->addMenu( bal );

	SetupMenuValFloat * bugs = new SetupMenuValFloat( "Bugs", &_setup->get()->_bugs, "% bugs", 0.0, 50, 1, bug_adj, true  );
	bugs->setHelp(PROGMEM"Percent of bugs contamination to indicate degradation of gliding performance");
	mm->addMenu( bugs );


	String elev_unit = "m";
	int step = 1;
	if( _setup->get()->_alt_unit == 1 ){ // ft
		step = 5;
	}

	SetupMenuValFloat * afe = new SetupMenuValFloat( "Airfield Elevation", &_setup->get()->_elevation, "m", -1, 3000, step, elev_adj, true  );
	afe->setHelp(PROGMEM"Set airfield elevation in meters for QNH auto adjust on ground according to this setting");
	mm->addMenu( afe );


// Vario
	SetupMenu * va = new SetupMenu( "Vario" );
	MenuEntry* vae = mm->addMenu( va );

	String vunit;
	if( _setup->get()->_vario_unit == 0 )
		vunit = "m/s";
	else if( _setup->get()->_vario_unit == 1 )
		vunit = "x 100ft/m";
	else if( _setup->get()->_vario_unit == 2 )
		vunit = "kt";
	SetupMenuValFloat * vga = new SetupMenuValFloat( 	"Range",
			&_setup->get()->_range,
			vunit.c_str(),
			1.0, 30.0,
			1, update_rentry );
	vga->setHelp(PROGMEM"Upper and lower value for Vario graphic display region");
	vga->setPrecision( 0 );
	vae->addMenu( vga );

	SetupMenuValFloat * vda = new SetupMenuValFloat( 	"Vario Bar Damping",
			&_setup->get()->_vario_delay,
			"sec",
			2.0, 10.0,
			0.1 );
	vda->setHelp(PROGMEM"Response time, time constant of Vario low pass kalman filter");
	vae->addMenu( vda );



	SetupMenuValFloat * vds2 = new SetupMenuValFloat( 	"S2F Damping",
			&_setup->get()->_s2f_delay,
			"sec",
			0.10001, 10.0,
			0.1 );
	vds2->setHelp(PROGMEM"Time constant of S2F low pass filter");
	vae->addMenu( vds2 );

	SetupMenuValFloat * vdav = new SetupMenuValFloat( 	"Average Vario Damping",
				&_setup->get()->_vario_av_delay,
				"sec",
				2.0, 60.0,
				0.1 );
	vdav->setHelp(PROGMEM"Response time, time constant of digital Average Vario Display");
	vae->addMenu( vdav );


	SetupMenuValFloat * vccm = new SetupMenuValFloat( "Mean Climb Minimum",
				&_setup->get()->_core_climb_min,
				"m/s",
				0.0, 2.0,
				0.1 );
	vccm->setHelp(PROGMEM"Minimum climb rate that counts for arithmetic mean climb value (red rhombus left of TE bar)");
    vae->addMenu( vccm );

    SetupMenuValFloat * vcch = new SetupMenuValFloat( "Mean Climb Period",
    				&_setup->get()->_core_climb_history,
    				"min",
    				1, 300,
    				1 );
    vcch->setHelp(PROGMEM"Number of minutes where samples for mean climb value are regarded, default is last 3 thermals a 15 min");
    vae->addMenu( vcch );


	SetupMenuSelect * sink = new SetupMenuSelect( 	"Polar Sink Display",
					&_setup->get()->_ps_display );
	sink->setHelp(PROGMEM"Show polar sink rate together with TE in Vario bar");
	sink->addEntry( "DISABLE");
	sink->addEntry( "ENABLE");
	vae->addMenu( sink );



// Audio
	SetupMenu * ad = new SetupMenu( "Audio" );
	MenuEntry* ade = mm->addMenu( ad );

	SetupMenuValFloat * dv = new SetupMenuValFloat( "Default Volume",
						&_setup->get()->_default_volume,
						"%",
						0, 100,
						1.0 );
	ade->addMenu( dv );
	dv->setHelp(PROGMEM"Default volume for Audio when device is switched on");

	SetupMenuSelect * dt = new SetupMenuSelect( 	"Tone Style", &_setup->get()->_dual_tone );
	dt->setHelp(PROGMEM"Select dual tone aka ilec sound, (di/da/di) or single tone (di-di-di) mode");
	dt->addEntry( "Single Tone");      // 0
	dt->addEntry( "Dual Tone");        // 1
	ade->addMenu( dt );

	SetupMenuSelect * tch = new SetupMenuSelect( 	"Tone Chopping", &_setup->get()->_chopping_mode );
	tch->setHelp(PROGMEM"Select tone chopping option for Vario and or S2F");
	tch->addEntry( "Disabled");             // 0
	tch->addEntry( "Vario only");           // 1
	tch->addEntry( "S2F only");             // 2
	tch->addEntry( "Vario and S2F");        // 3  default
	ade->addMenu( tch );

	SetupMenuValFloat * htv = new SetupMenuValFloat( 	"Dual Tone Pich",
						&_setup->get()->_high_tone_var,
						"%",
						0, 50,
						1.0 );
	htv->setHelp(PROGMEM"Tone variation in Dual Tone mode, percent of frequency pitch up for second tone");
	ade->addMenu( htv );


	SetupMenuSelect * am = new SetupMenuSelect( 	"Audio Mode",
				&_setup->get()->_audio_mode, false );
	am->setHelp( PROGMEM"Selects audio source. Audio either follows Vario, or S2F exclusively, controlled by external switch or automatically by speed" );
		am->addEntry( "Vario");
		am->addEntry( "S2F");
		am->addEntry( "Switch");
		am->addEntry( "AutoSpeed");
	ade->addMenu( am );

	SetupMenuValFloat * ts = new SetupMenuValFloat( 	"AutoSpeed",
					&_setup->get()->_s2f_speed,
					"km/h",
					30.0, 200.0,
					1.0 );
	ade->addMenu( ts );
	ts->setHelp(PROGMEM"Transition speed when audio changes to S2F mode in AutoSpeed mode in metric system. 100 km/h equals rounfd 62 mph or 54 knots");

	SetupMenuValFloat * cf = new SetupMenuValFloat( 	"CenterFreq",
			&_setup->get()->_center_freq,
			"Hz",
			200.0, 2000.0,
			10.0 );
	cf->setHelp(PROGMEM"Center frequency for Audio at zero Vario or zero S2F delta");
	ade->addMenu( cf );
	SetupMenuValFloat * oc = new SetupMenuValFloat( 	"Octaves",
			&_setup->get()->_tone_var,
			"fold",
			1.5, 4,
			0.1 );
	oc->setHelp(PROGMEM"Maximum tone frequency variation");
	ade->addMenu( oc );


	SetupMenuSelect * ar = new SetupMenuSelect( 	"Range",
						&_setup->get()->_audio_range );

	audio_range_sm = ar;
	sprintf( rentry, "Variable (%d m/s)", (int)(_setup->get()->_range) );
	ar->addEntry( "Max eq. 5 m/s");
	ar->addEntry( rentry );
	ar->setHelp(PROGMEM"Select either fix (5m/s) or variable Audio range according to current vario setting");
	ade->addMenu( ar );

	SetupMenu * db = new SetupMenu( "Deadband" );
	MenuEntry* dbe = ade->addMenu( db );
	dbe->setHelp(PROGMEM"Audio dead band limits within Audio remains silent in metric scale. 0,1 m/s equals roughtly 20 ft/min or 0.2 knots");

	SetupMenuValFloat * dbminlv = new SetupMenuValFloat( 	"Lower Value Vario",
		&_setup->get()->_deadband_neg,
					"m/s",
					-5.0, 0,
					0.1 );
	dbminlv->setHelp(PROGMEM"Lower limit for Audio mute function");
	dbe->addMenu( dbminlv );

	SetupMenuValFloat * dbmaxlv = new SetupMenuValFloat( 	"Upper Value Vario",
			&_setup->get()->_deadband,
			"m/s",
			0, 5.0,
			0.1 );
	dbmaxlv->setHelp(PROGMEM"Upper limit for Audio mute function");
	dbe->addMenu( dbmaxlv );

	SetupMenuValFloat * dbmaxls2f = new SetupMenuValFloat( 	"S2F Deadband",
			&_setup->get()->_s2f_deadband,
			"+-km/h",
			0, 25.0,
			1 );
	dbmaxls2f->setHelp(PROGMEM"Positive and negative limit in speed deviation for S2F mute function");
	dbe->addMenu( dbmaxls2f );



// Polar Setup
	SetupMenu * po = new SetupMenu( "Polar" );
	po->setHelp( PROGMEM"Polar setup to match performance of glider");
	MenuEntry* poe = mm->addMenu( po );


	SetupMenuSelect * glt = new SetupMenuSelect( 	"Glider-Type",
						&_setup->get()->_glider_type, false, polar_select );
	poe->addMenu( glt );

	printf( "Number of Polars installed: %d\n", Polars::numPolars() );
	for( int x=0; x< Polars::numPolars(); x++ ){
		glt->addEntry( Polars::getPolar(x).type );
	}

	SetupMenu * pa = new SetupMenu( "PolarAdjust" );
	pa->setHelp(PROGMEM "Adjust Polar at 3 points of selected polar in commonly used metric system for Polars", 230 );
	poe->addMenu( pa );

	SetupMenuValFloat * wil = new SetupMenuValFloat(
				"Wingload", &_setup->get()->_polar.wingload, "kg/m2", 10.0, 100.0, 0.1, polar_adj );
	pa->addMenu( wil );
	SetupMenuValFloat * pov1 = new SetupMenuValFloat(
			"Speed 1", &_setup->get()->_polar.speed1, "km/h", 50.0, 120.0, 1, polar_adj );
	pov1->setHelp(PROGMEM"Speed 1, near to minimum sink from polar e.g. 80 km/h");
	pa->addMenu( pov1 );
	SetupMenuValFloat * pos1 = new SetupMenuValFloat(
			"Sink  1", &_setup->get()->_polar.sink1, "m/s", -3.0, 0.0, 0.01, polar_adj );
	pos1->setHelp(PROGMEM"Sink indication at Speed 1 from polar");
	pa->addMenu( pos1 );
	SetupMenuValFloat * pov2 = new SetupMenuValFloat(
			"Speed 2", &_setup->get()->_polar.speed2, "km/h", 100.0, 180.0, 1, polar_adj );
	pov2->setHelp(PROGMEM"Speed 2 for a moderate cruise from polar e.g. 120 km/h");
	pa->addMenu( pov2 );
	SetupMenuValFloat * pos2 = new SetupMenuValFloat(
			"Sink  2", &_setup->get()->_polar.sink2, "m/s", -5.0, 0.0, 0.01, polar_adj );
	pos2->setHelp(PROGMEM"Sink indication at Speed 2 from polar");
	pa->addMenu( pos2 );
	SetupMenuValFloat * pov3 = new SetupMenuValFloat(
			"Speed 3", &_setup->get()->_polar.speed3, "km/h", 120.0, 250.0, 1, polar_adj );
	pov3->setHelp(PROGMEM"Speed 3 for a fast cruise from polar e.g. 170 km/h");
	pa->addMenu( pov3 );
	SetupMenuValFloat * pos3 = new SetupMenuValFloat(
			"Sink  3", &_setup->get()->_polar.sink3, "m/s", -6.0, 0.0, 0.01, polar_adj );
	pos3->setHelp(PROGMEM"Sink indication at Speed 3 from polar");
	pa->addMenu( pos3 );

	SetupMenuValFloat * maxbal = new SetupMenuValFloat(
				"Max Ballast", &_setup->get()->_polar.max_ballast, "liters", 0, 500, 1 );
	maxbal->setHelp(PROGMEM"Maximum water ballast for selected glider to allow sync from XCSoar using fraction of max ballast");
	poe->addMenu( maxbal );



	SetupMenu * wk = new SetupMenu( "Flap (WK) Indicator" );
	MenuEntry* wkm = mm->addMenu( wk );
	SetupMenuSelect * wke = new SetupMenuSelect( "Flap Indicator Option", &_setup->get()->_flap_enable, true );
	wke->addEntry( "Disable");
	wke->addEntry( "Enable");
	wke->setHelp(PROGMEM"Option to enable Flap (WK) Indicator to assist optimum flap setting depending on speed and ballast");
	wkm->addMenu( wke );

	SetupMenuValFloat * plus1 = new SetupMenuValFloat("Speed +2 to +1", &_setup->get()->_flap_plus_1, "km/h",  50, 150, 1 );
	plus1->setHelp(PROGMEM"Speed for transition from +2 to +1 flap setting");
	wkm->addMenu( plus1 );
	SetupMenuValFloat * zero = new SetupMenuValFloat("Speed +1 to 0", &_setup->get()->_flap_0, "km/h",  50, 150, 1 );
	zero->setHelp(PROGMEM"Speed for transition from +1 to 0 flap setting");
	wkm->addMenu( zero );
	SetupMenuValFloat * min1 = new SetupMenuValFloat("Speed 0 to -1", &_setup->get()->_flap_minus_1, "km/h",  80, 180, 1 );
	min1->setHelp(PROGMEM"Speed for transition from 0 to -1 flap setting");
	wkm->addMenu( min1 );
	SetupMenuValFloat * min2 = new SetupMenuValFloat("Speed -1 to -2", &_setup->get()->_flap_minus_2, "km/h",  100, 280, 1 );
	min2->setHelp(PROGMEM"Speed for transition from -1 to -2 flap setting");
	wkm->addMenu( min2 );

	SetupMenu * sy = new SetupMenu( "System" );
	MenuEntry* sye = mm->addMenu( sy );

	SetupMenu * soft = new SetupMenu( "Software Update" );
	sye->addMenu( soft );

	Version V;
	static uint8_t select_dummy = 0;
	SetupMenuSelect * ver = new SetupMenuSelect( "Software Version", &select_dummy, false, 0, false );
	ver->addEntry( V.version() );
	soft->addMenu( ver );

	SetupMenuSelect * upd = new SetupMenuSelect( "Software Update", &_setup->get()->_software_update, true, 0, true );
	soft->addMenu( upd );
	upd->setHelp(PROGMEM "Software Update over the air (OTA). Start Wifi AP, then connect to Wifi 'ESP32 OTA' and open http://192.161.0.1 to upload firmware");
	upd->addEntry( "Cancel");
	upd->addEntry( "Start Wifi AP");

	// Bluetooth
	String btname="Bluetooth   ";
	btname += _setup->getID();

	SetupMenuSelect * btm = new SetupMenuSelect(  btname, &_setup->get()->_blue_enable, true );
	btm->addEntry( "Sender OFF");
	btm->addEntry( "Sender ON");
	sye->addMenu( btm );

	float fva = _setup->get()->_factory_volt_adjust;
	if( abs(fva - 0.00815) < 0.00001 ) {
		printf("Add option to Factory adjust ADC; not yet done\n");
		SetupMenuValFloat * fvoltadj = new SetupMenuValFloat( 	"Factory Voltmeter Adj",
						&_setup->get()->_factory_volt_adjust,
						"%",
						-25.0, 25.0,
						0.01,
						factv_adj);
		fvoltadj->setHelp(PROGMEM "Option to fine factory adjust voltmeter");
		sye->addMenu( fvoltadj );
	}

	SetupMenuSelect * fa = new SetupMenuSelect( "Factory Reset", &_setup->get()->_factory_reset, true );
	fa->setHelp(PROGMEM "Option to reset all settings to factory defaults, means metric system, 5m/s vario range and more");
	fa->addEntry( "Cancel");
	fa->addEntry( "ResetAll");
	sye->addMenu( fa );

	SetupMenu * bat = new SetupMenu( "Battery Setup" );
	bat->setHelp( PROGMEM "Adjust corresponding voltage for battery symbol display low,red,yellow and full");
    sye->addMenu( bat );

	SetupMenuValFloat * blow = new SetupMenuValFloat(
				"Battery Low", &_setup->get()->_bat_low_volt, "Volt ", 0.0, 28.0, 0.1 );
	SetupMenuValFloat * bred = new SetupMenuValFloat(
				"Battery Red", &_setup->get()->_bat_red_volt, "Volt ", 0.0, 28.0, 0.1  );
	SetupMenuValFloat * byellow = new SetupMenuValFloat(
				"Battery Yellow", &_setup->get()->_bat_yellow_volt, "Volt ", 0.0, 28.0, 0.1 );
	SetupMenuValFloat * bfull = new SetupMenuValFloat(
				"Battery Full", &_setup->get()->_bat_full_volt, "Volt ", 0.0, 28.0, 0.1  );

	SetupMenuSelect * batv = new SetupMenuSelect( "Battery Display", &_setup->get()->_battery_display );
	batv->setHelp(PROGMEM "Option to display battery charge state either in Percentage e.g. 75% or Voltage e.g. 12.5V");
	batv->addEntry( "Percentage");
	batv->addEntry( "Voltage");

	bat->addMenu(blow);
	bat->addMenu(bred);
	bat->addMenu(byellow);
	bat->addMenu(bfull);
	bat->addMenu( batv );

	SetupMenu * display = new SetupMenu( "DISPLAY Setup" );
	sye->addMenu( display );
	 // UNIVERSAL, RAYSTAR_RFJ240L_40P, ST7789_2INCH_12P, ILI9341_TFT_18P
	if( _setup->get()->_display_type == UNIVERSAL )
	{
		SetupMenuSelect * dtype = new SetupMenuSelect( 	"Display Type", &_setup->get()->_display_type );
		dtype->setHelp( PROGMEM "Factory setup for corresponding display type used");
		dtype->addEntry( "UNIVERSAL");
		dtype->addEntry( "RAYSTAR_RFJ240L_40P");
		dtype->addEntry( "ST7789_2INCH_12P");
		dtype->addEntry( "ILI9341_TFT_18P");
		display->addMenu( dtype );
	}
    // Orientation   _display_orientation
	SetupMenuSelect * diso = new SetupMenuSelect( "Display Orientation",	&_setup->get()->_display_orientation, true );
	display->addMenu( diso );
	diso->setHelp( PROGMEM "Display Orientation either NORMAL means control panel is right, or TOPDOWN means control panel is left");
	diso->addEntry( "NORMAL (Rotary left)");
	diso->addEntry( "TOPDOWN (Rotary right)");

	// Altimeter, IAS
	SetupMenu * aia = new SetupMenu( "Altimeter, Airspeed" );
	sye->addMenu( aia );
	SetupMenuSelect * als = new SetupMenuSelect( 	"Altimeter Source",	&_setup->get()->_alt_select );
	aia->addMenu( als );
	als->setHelp( PROGMEM "Select Source of altimeter to either barometric or static pressure sensor (default), or TE sensor what results in an 'energy' altitude");
	als->addEntry( "TE   Sensor");
	als->addEntry( "Baro Sensor");

	SetupMenuValFloat * spc = new SetupMenuValFloat( "IAS Calibration", &_setup->get()->_speedcal, "%", -10, 10, 1, 0, false  );
	spc->setHelp(PROGMEM"Calibration of indicated airspeed (IAS). Normally not needed, hence pressure probes may have systematic error");
	aia->addMenu( spc );

	SetupMenuSelect * amode = new SetupMenuSelect( 	"Airspeed Mode",	&_setup->get()->_airspeed_mode );
	aia->addMenu( amode );
	amode->setHelp( PROGMEM "Select mode of Airspeed indicator to display IAS (Indicated AirSpeed, default) or TAS (True AirSpeed) considering air density");
	amode->addEntry( "IAS");
	amode->addEntry( "TAS");


	// Units
	SetupMenu * un = new SetupMenu( "Units" );
	un->setHelp( PROGMEM "Setup altimeter, airspeed indicator and variometer with European Metric, American, British or Australian units");
	SetupMenuSelect * alu = new SetupMenuSelect( 	"Altimeter",	&_setup->get()->_alt_unit, true );
	alu->addEntry( "Meter       (m)");
	alu->addEntry( "Foot        (ft)");
	alu->addEntry( "FlightLevel (FL)");
	un->addMenu( alu );
	SetupMenuSelect * iau = new SetupMenuSelect( 	"Indicated Airspeed",	&_setup->get()->_ias_unit, true );
	iau->addEntry( "Km per hour     (Km/h)");
	iau->addEntry( "Miles  per hour (mph)");
	iau->addEntry( "Knots               (kt)");
	un->addMenu( iau );
	SetupMenuSelect * vau = new SetupMenuSelect( 	"Vario",	&_setup->get()->_vario_unit, true );
	vau->addEntry( "Meters/sec   (m/s)");
	vau->addEntry( "Foot per min (ft/min)");
	vau->addEntry( "Knots        (knots)");
	un->addMenu( vau );
	sye->addMenu( un );

	// Rotary Default
	SetupMenuSelect * rd = new SetupMenuSelect( 	"Rotary Default",	&_setup->get()->_rot_default );
	sye->addMenu( rd );
	rd->setHelp(PROGMEM "Select value to be altered at rotary movement outside of setup menu");
	rd->addEntry( "Volume");
	rd->addEntry( "MC Value");

	// _serial2_speed
	SetupMenu * rs232 = new SetupMenu( "RS232 Serial Interface" );
	sye->addMenu( rs232 );
	SetupMenuSelect * s2sp = new SetupMenuSelect( PROGMEM "Serial RS232 Speed",	&_setup->get()->_serial2_speed, true );
	rs232->addMenu( s2sp );
	// s2sp->setHelp( "Serial RS232 (TTL) speed, pins RX:2, TX:3 on external RJ45 connector");
	s2sp->addEntry( "Serial OFF");
	s2sp->addEntry( "4800 baud");
	s2sp->addEntry( "9600 baud");
	s2sp->addEntry( "19200 baud");
	s2sp->addEntry( "38400 baud");
	s2sp->addEntry( "57600 baud");
	s2sp->addEntry( "115200 baud");
	SetupMenuSelect * s2lo = new SetupMenuSelect( PROGMEM "Serial BT Bridge",	&_setup->get()->_serial2_rxloop, true );
	rs232->addMenu( s2lo );
	s2lo->setHelp( "Serial RS232 (TTL) option to retransmit data on serial RX (pin2 RJ45), to bluetooth sender to connect e.g. FLARM");
	s2lo->addEntry( "Disable");
	s2lo->addEntry( "Enable");

	SetupMenuSelect * sout = new SetupMenuSelect( PROGMEM "Serial TX",	&_setup->get()->_serial2_tx, true );
	rs232->addMenu( sout );
	sout->setHelp( "Serial RS232 (TTL) option to transmit OpenVario NMEA and or data from BT on serial TX (pin3 RJ45) to connect serial devices");
	sout->addEntry( "Disable all");
	sout->addEntry( "Enable OpenVario");
	sout->addEntry( "Enable BT Bridge");
	sout->addEntry( "Enable all");

	SetupMenuSelect * stxi = new SetupMenuSelect( PROGMEM "Serial TX Inversion",	&_setup->get()->_serial2_tx_inverted, true );
	rs232->addMenu( stxi );
	stxi->setHelp( "Serial RS232 (TTL) option for negative logic, means a '1' will be sent at zero level (RS232 standard and default) and vice versa");
	stxi->addEntry( "Normal");
	stxi->addEntry( "Inverted");

	SetupMenuSelect * srxi = new SetupMenuSelect( PROGMEM "Serial RX Inversion",	&_setup->get()->_serial2_rx_inverted, true );
	rs232->addMenu( srxi );
	srxi->setHelp( "Serial RS232 (TTL) option for negative logic, means a '1' will be received at zero level (RS232 standard and default) and vice versa");
	srxi->addEntry( "Normal");
	srxi->addEntry( "Inverted");

	SetupMenuSelect * nmea = new SetupMenuSelect( PROGMEM "NMEA Protocol",	&_setup->get()->_nmea_protocol, false );
	sye->addMenu( nmea );
	nmea->setHelp( "Setup protocol used for NMEA sending what corresponds to the driver used in OpenVario");
	nmea->addEntry( "OpenVario");
	nmea->addEntry( "Borgelt");

	SetupMenu::display();
}

SetupMenuValFloat::SetupMenuValFloat( String title, float *value, String unit, float min, float max, float step, int (*action)( SetupMenuValFloat *p ), bool end_menu ) {
	// printf("SetupMenuValFloat( %s ) \n", title.c_str() );
	_rotary->attach(this);
	_title = title;
	highlight = -1;
	_value = value;
	_unit = unit;
	_min = min;
	_max = max;
	_step = step;
	_action = action;
	_end_menu = end_menu;
	_precision = 2;
}

void SetupMenuValFloat::setPrecision( int prec ){
	_precision = prec;
}


void MenuEntry::clear()
{
	printf("MenuEntry::clear\n");
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ucg->setColor(COLOR_BLACK);
	ucg->drawBox( 0,0,240,320 );
	// ucg->begin(UCG_FONT_MODE_SOLID);
	ucg->setFont(ucg_font_ncenR14_hr);
	ucg->setPrintPos( 1, 30 );
	ucg->setColor(COLOR_WHITE);
	xSemaphoreGive(spiMutex );
}


SetupMenuValFloat * SetupMenuValFloat::qnh_menu = 0;

void SetupMenuValFloat::showQnhMenu(){
	printf("SetupMenuValFloat::showQnhMenu()\n");
	if( qnh_menu ) {
		printf("qnh_menu = true\n");
		_menu_enabled = true;
		selected = qnh_menu;
		Audio.disable();
		qnh_menu->clear();
		qnh_menu->display();
		qnh_menu->pressed = true;
	}
}

void SetupMenuValFloat::display( int mode ){
	if( (selected != this) || !_menu_enabled )
		return;
	// printf("SetupMenuValFloat display() %d %x\n", pressed, (int)this);
	uprintf( 5,25, selected->_title.c_str() );
	displayVal();
	y= 75;
	if( _action != 0 )
		(*_action)( this );

	showhelp( y );
	if(mode == 1){
		y+=24;
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		ucg->setPrintPos( 1, 300 );
		ucg->print("Saved");
		xSemaphoreGive(spiMutex );
	}
	y=0;

	if( mode == 1 )
	    vTaskDelay(1000 / portTICK_PERIOD_MS);
	printf("~SetupMenuValFloat display\n");
}

void SetupMenuValFloat::displayVal()
{
	ucg->setFont(ucg_font_fub25_hr);
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ucg->setPrintPos( 1, 70 );

	ucg->printf("%0.*f %s   ", _precision, *_value, _unit.c_str());
	xSemaphoreGive(spiMutex );
	ucg->setFont(ucg_font_ncenR14_hr);
}

void SetupMenuValFloat::down( int count ){
	if( (selected != this) || !_menu_enabled )
		return;
	printf("val down %d times \n", count );
	while( (*_value > _min) && count ) {
		*_value -= _step;
		count --;
	}
	displayVal();
	if( _action != 0 )
		(*_action)( this );
}

void SetupMenuValFloat::up( int count ){
	if( (selected != this) || !_menu_enabled )
		return;
	printf("val up %d times \n", count );
	while( (*_value < _max) && count ) {
		*_value += _step;
		count--;
	}
    displayVal();
	if( _action != 0 )
		(*_action)( this );
}

void SetupMenuValFloat::press(){
	if( selected != this )
		return;
	printf("SetupMenuValFloat press\n");
	if ( pressed ){
		display( 1 );
		if( _end_menu )
			selected = root;
		else if( _parent != 0 )
			selected = _parent;
		selected->highlight = -1;  // to topmost selection when back
		selected->pressed = true;
		_setup->commit();
		pressed = false;
		BMPVario::setHolddown( 150 );  // so seconds stop average
		if( _end_menu )
			selected->press();
	}
	else
	{
		pressed = true;
		clear();
		display();
	}
}


SetupMenuSelect::SetupMenuSelect( String title, uint8_t *select, bool restart, int (*action)(SetupMenuSelect *p), bool save ) {
	// printf("SetupMenuSelect( %s ) \n", title.c_str() );
	_rotary->attach(this);
	_title = title;
	highlight = -1;
	_select = select;
	_select_save = *select;
	_numval = 0;
	_restart = restart;
	_action = action;
	_save = save;
}


void SetupMenuSelect::display( int mode ){
	if( (selected != this) || !_menu_enabled )
		return;
	printf("SetupMenuSelect display() %d %x\n", pressed, (int)this);
	clear();
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ucg->setPrintPos(1,25);
	printf("Title: %s y=%d\n", _title.c_str(),y );
	ucg->printf("<< %s",_title.c_str());
	xSemaphoreGive(spiMutex );
	printf("select=%d numval=%d\n", *_select, _numval );
	if( _numval > 9 ){
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		ucg->setPrintPos( 1, 50 );
		ucg->printf( "%s                ", _values[*_select].c_str() );
		xSemaphoreGive(spiMutex );
	}else
	{
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		for( int i=0; i<_numval && i<+10; i++ )	{
			ucg->setPrintPos( 1, 50+25*i );
			ucg->print( _values[i].c_str() );
		}
		ucg->drawFrame( 1,(*_select+1)*25+3,238,25 );
		xSemaphoreGive(spiMutex );
	}

	y=_numval*25+50;
	showhelp( y );
	if(mode == 1 && _save == true ){
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		ucg->setPrintPos( 1, 300 );
		ucg->print("Saved !" );
		if( _select_save != *_select )
			if( _restart ) {
				ucg->setColor(COLOR_BLACK);
				ucg->drawBox( 0,160,240,160 );
				ucg->setPrintPos( 1, 250  );
				ucg->setColor(COLOR_WHITE);
				ucg->print("Now Restart" );
			}
		xSemaphoreGive(spiMutex );
	}
	if( mode == 1 )
		delay(1000);
}

void SetupMenuSelect::down(int count){
	if( (selected != this) || !_menu_enabled )
		return;

	if( _numval > 9 ){
		    xSemaphoreTake(spiMutex,portMAX_DELAY );
			while( count ) {
				if( (*_select) > 0 )
					(*_select)--;
				count--;
			}
			ucg->setPrintPos( 1, 50 );
			ucg->printf("%s                  ",_values[*_select].c_str());
			xSemaphoreGive(spiMutex );
	}else {
		ucg->setColor(COLOR_BLACK);
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		ucg->drawFrame( 1,(*_select+1)*25+3,238,25 );  // blank old frame
		ucg->setColor(COLOR_WHITE);
		if( (*_select) >  0 )
			(*_select)--;
		printf("val down %d\n", *_select );
		ucg->drawFrame( 1,(*_select+1)*25+3,238,25 );  // draw new frame
		xSemaphoreGive(spiMutex );
	}
}

void SetupMenuSelect::up(int count){
	if( (selected != this) || !_menu_enabled )
		return;
	if( _numval > 9 )
	{
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		while( count ) {
			if( (*_select) <  _numval-1 )
				(*_select)++;
			count--;
			}
		ucg->setPrintPos( 1, 50 );
		ucg->printf("%s                   ", _values[*_select].c_str());
		xSemaphoreGive(spiMutex );
	}else {
		xSemaphoreTake(spiMutex,portMAX_DELAY );
		ucg->setColor(COLOR_BLACK);
		ucg->drawFrame( 1,(*_select+1)*25+3,238,25 );  // blank old frame
		ucg->setColor(COLOR_WHITE);
		if ( (*_select) < _numval-1 )
			(*_select)++;
		printf("val up %d\n", *_select );
		ucg->drawFrame( 1,(*_select+1)*25+3,238,25 );  // draw new frame
		xSemaphoreGive(spiMutex );
	}
}

void SetupMenuSelect::press(){
	if( selected != this )
		return;
	printf("SetupMenuSelect press\n");
	if ( pressed ){
		display( 1 );
		if( _parent != 0)
			selected = _parent;
		_parent->highlight = -1;  // to topmost selection when back
		selected->pressed = true;
		_setup->commit();
		pressed = false;
		if( _action != 0 )
			(*_action)( this );
		if( _select_save != *_select )
			if( _restart ) {
				sleep( 2 );
				esp_restart();
			}
	}
	else
	{
		pressed = true;
	}
}
