/*
 * Setup.h
 *
 *  Created on: August 23, 2020
 *      Author: iltis
 */

#ifndef MAIN_SETUP_NG_H_
#define MAIN_SETUP_NG_H_

extern "C" {
#include "esp_partition.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
}

#include <string>
#include "esp_system.h"
#include "stdio.h"
#include <esp_log.h>
#include <string>
#include "string.h"
#include "BTSender.h"
#include "Polars.h"
#include <iostream>
#include <vector>
#include <logdef.h>



/*
 *
 * NEW Simplified and distributed non volatile config data API with template classes:
 * SetupNG<int>  airspeed_mode( "AIRSPEED_MODE", MODE_IAS );
 *
 *  int as = airspeed_mode.get();
 *  airspeed_mode.set( MODE_TAS );
 *
 */


typedef enum display_type { UNIVERSAL, RAYSTAR_RFJ240L_40P, ST7789_2INCH_12P, ILI9341_TFT_18P } display_t;
typedef enum chopping_mode { NO_CHOP, VARIO_CHOP, S2F_CHOP, BOTH_CHOP } chopping_mode_t;
typedef enum rs232linemode { RS232_NORMAL, RS232_INVERTED } rs232lm_t;
typedef enum nmea_protocol  { OPENVARIO, BORGELT } nmea_proto_t;
typedef enum airspeed_mode  { MODE_IAS, MODE_TAS } airspeed_mode_t;

class SetupCommon {
public:
	SetupCommon() {  memset( _ID, 0, sizeof( _ID )); };
	~SetupCommon() {};
	virtual bool init() = 0;
	virtual bool erase() = 0;
	virtual const char* key() = 0;
	static std::vector<SetupCommon *> entries;
	static void initSetup();
	static char *getID();
private:
	static char _ID[14];
};

template<typename T>

class SetupNG: public SetupCommon
{
public:
	inline T* getPtr() {
		return &_value;
	};
	inline T& getRef() {
		return _value;
	};
	inline T get() {
		return _value;
	};
	const char * key() {
		return _key;
	}
	bool set( T aval ) {
		std::cout << "set( "<< aval << " )\n";
		_value = T(aval);
		if( !open() ) {
			ESP_LOGE(FNAME,"NVS Error open nvs handle !");
			return( false );
		}
		bool ret = commit();
		return ret;
	};
	bool commit() {
		ESP_LOGI(FNAME,"NVS commit(): ");
		if( !open() ) {
			ESP_LOGE(FNAME,"NVS error");
			return false;
		}
		ESP_LOGI(FNAME,"NVS commit(key:%s , addr:%08x, len:%d, nvs_handle: %04x)", _key, (unsigned int)(&_value), sizeof( _value ), _nvs_handle);
		esp_err_t _err = nvs_set_blob(_nvs_handle, _key, (void *)(&_value), sizeof( _value ));
		if(_err != ESP_OK) {
			ESP_LOGE(FNAME,"NVS set blob error %d", _err );
			close();
			return( false );
		}
		_err = nvs_commit(_nvs_handle);
		if(_err != ESP_OK)  {
			ESP_LOGE(FNAME,"NVS nvs_commit error");
			close();
			return false;
		}
		ESP_LOGI(FNAME,"success");
		close();
		return true;
	};
	bool init() {
		if( !open() ) {
			ESP_LOGE(FNAME,"Error open nvs handle !");
			return false;
		}
		size_t required_size;
		esp_err_t _err = nvs_get_blob(_nvs_handle, _key, NULL, &required_size);
		if ( _err != ESP_OK ){
			ESP_LOGE(FNAME, "NVS nvs_get_blob error: returned error ret=%d", _err );
			set( _default );  // try to init
		}
		else {
			if( required_size > sizeof( T ) ) {
				ESP_LOGE(FNAME,"NVS error: size too big: %d > %d", required_size , sizeof( T ) );
				erase();
				set( _default );  // try to init
				close();
				return false;
			}
			else {
				// ESP_LOGI(FNAME,"NVS size okay: %d", required_size );
				_err = nvs_get_blob(_nvs_handle, _key, &_value, &required_size);
				if ( _err != ESP_OK ){
					ESP_LOGE(FNAME, "NVS nvs_get_blob returned error ret=%d", _err );
					erase();
					set( _default );  // try to init
				}
				else {
					ESP_LOGI(FNAME,"NVS key %s exists len: %d value: %f0.1",_key, required_size, (float)_value );
					// std::cout << _value << "\n";
				}
			}
		}
		close();
		return true;
	};
	SetupNG() {};
	SetupNG( const char * akey, T adefault ) {
		// ESP_LOGI(FNAME,"SetupNG(%s)", akey );
		entries.push_back( this );  // an into vector
		_nvs_handle = 0;
		_key = akey;
		_default = adefault;

	};

	bool erase_all() {
		open();
		esp_err_t _err = nvs_erase_all(_nvs_handle);
		if(_err != ESP_OK)
			return false;
		else
			ESP_LOGI(FNAME,"NVS erased all by handle %d", _nvs_handle );
		if( commit() )
			return true;
		else
			return false;

	};

	bool erase() {
		open();
		esp_err_t _err = nvs_erase_key(_nvs_handle, _key);
		if(_err != ESP_OK)
			return false;
		else {
			ESP_LOGI(FNAME,"NVS erased %s by handle %d", _key, _nvs_handle );
			if( set( _default ) )
				return true;
			else
				return false;
		}
	};


private:
	T _value;
	T _default;
	const char * _key;
	nvs_handle_t  _nvs_handle;

	bool open() {
		if( _nvs_handle == 0) {
			esp_err_t _err = nvs_open("SetupNG", NVS_READWRITE, &_nvs_handle);
			if(_err != ESP_OK){
				ESP_LOGE(FNAME,"ESP32NVS open storage error %d", _err );
				_nvs_handle = 0;
				return( false );
			}
			else {
				// ESP_LOGI(FNAME,"ESP32NVS handle: %04X  OK", _nvs_handle );
				return( true );
			}
		}
		return true;
	};
	void close() {
		if( _nvs_handle != 0) {
			nvs_close(_nvs_handle);
			_nvs_handle = 0;
		}
	};
};


extern SetupNG<float>  		QNH;
extern SetupNG<float> 		polar_wingload;
extern SetupNG<float> 		polar_speed1;
extern SetupNG<float> 		polar_sink1;
extern SetupNG<float> 		polar_speed2;
extern SetupNG<float> 		polar_sink2;
extern SetupNG<float> 		polar_speed3;
extern SetupNG<float> 		polar_sink3;
extern SetupNG<float> 		polar_max_ballast;
extern SetupNG<float> 		polar_wingarea;

extern SetupNG<float>  		speedcal;
extern SetupNG<float>  		vario_delay;
extern SetupNG<float>  		vario_av_delay;
extern SetupNG<float>  		center_freq;
extern SetupNG<float>  		tone_var;
extern SetupNG<int>  		dual_tone;
extern SetupNG<float>  		high_tone_var;
extern SetupNG<float>  		deadband;
extern SetupNG<float>  		deadband_neg;
extern SetupNG<float>  		range;
extern SetupNG<float>  		ballast;
extern SetupNG<float>  		MC;
extern SetupNG<float>  		s2f_speed;

extern SetupNG<int>  		audio_mode;
extern SetupNG<int>  		chopping_mode;

extern SetupNG<int>  		blue_enable;
extern SetupNG<int>  		factory_reset;
extern SetupNG<int>  		audio_range;
extern SetupNG<int>  		alt_select;
extern SetupNG<int>  		fl_auto_transition;
extern SetupNG<float>  		transition_alt;
extern SetupNG<int>  		glider_type;
extern SetupNG<int>  		ps_display;

extern SetupNG<float>  		as_offset;
extern SetupNG<float>  		bat_low_volt;
extern SetupNG<float>  		bat_red_volt;
extern SetupNG<float>  		bat_yellow_volt;
extern SetupNG<float>  		bat_full_volt;
extern SetupNG<float>  		core_climb_min;
extern SetupNG<float>  		core_climb_history;
extern SetupNG<float>  		elevation;
extern SetupNG<float>  		default_volume;
extern SetupNG<float>  		s2f_deadband;
extern SetupNG<float>  		s2f_delay;
extern SetupNG<float>  		factory_volt_adjust;
extern SetupNG<float>  		bugs;

extern SetupNG<int>  		display_type;
extern SetupNG<int>  		display_orientation;
extern SetupNG<int>  		flap_enable;
extern SetupNG<float>  		flap_minus_2;
extern SetupNG<float>  		flap_minus_1;
extern SetupNG<float>  		flap_0;
extern SetupNG<float>  		flap_plus_1;
extern SetupNG<int>  		alt_unit;
extern SetupNG<int>  		ias_unit;
extern SetupNG<int>  		vario_unit;
extern SetupNG<int>  		rot_default;

extern SetupNG<int>  		serial2_speed;
extern SetupNG<int>  		serial2_rxloop;
extern SetupNG<int>  		serial2_tx;
extern SetupNG<int>  		serial2_tx_inverted;
extern SetupNG<int>  		serial2_rx_inverted;
extern SetupNG<int>  		software_update;
extern SetupNG<int>  		battery_display;
extern SetupNG<int>  		airspeed_mode;
extern SetupNG<int>  		nmea_protocol;
extern SetupNG<int>		    log_level;
extern SetupNG<float>		audio_factor;
extern SetupNG<float>		te_comp_adjust;
extern SetupNG<int>		    te_comp_enable;
extern SetupNG<int>		    rotary_dir;
extern SetupNG<int>		    rotary_inc;



#endif /* MAIN_SETUP_NG_H_ */
