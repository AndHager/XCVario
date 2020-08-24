/*
 * BatVoltage.cpp
 *
 *  Created on: Mar 18, 2018
 *      Author: iltis
 */



#include "driver/adc.h"
#include "BatVoltage.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc_cal.h"

#define DIODE_VOLTAGE_DROP 0        // New Vario now measures behind Diode


BatVoltage::BatVoltage() {
  _battery = 0;
  _battery_ch = ADC1_CHANNEL_MAX;
  _reference_ch = ADC1_CHANNEL_MAX;
  _adc_reference = 0;
  adc_chars = 0;
}

esp_adc_cal_characteristics_t adc_cal;

void BatVoltage::begin( adc1_channel_t battery, adc1_channel_t reference ) {
	_battery_ch = battery;
	_reference_ch = reference;
	adc1_config_width(ADC_WIDTH_BIT_12);
	adc1_config_channel_atten(battery,ADC_ATTEN_DB_0);

	adc_chars = (esp_adc_cal_characteristics_t *) &adc_cal;
	esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_0, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
	//Check type of calibration value used to characterize ADC
	printf("\n");
	if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
		printf("eFuse Vref\n");
	} else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
		printf("Two Point\n");
	} else {
		printf("Default\n");
	}
	getBatVoltage( true );
}

float BatVoltage::getBatVoltage( bool init ){
	float adc = 0.0;
	for( int i=0; i<1000; i++ ) {
	   adc += adc1_get_raw( _battery_ch );
	}
	adc = adc/1000.0;
	uint32_t voltage = esp_adc_cal_raw_to_voltage( (int)(adc+0.5), adc_chars);
    // printf("Voltage %d\n", voltage);

	float bat = (float)voltage * _correct * ( (100.0 + factory_volt_adjust.get()) / 100.0 ) +  DIODE_VOLTAGE_DROP;
	if( init )
		_battery = bat;
	else
		_battery = _battery + ( bat - _battery ) * 0.2;
	return _battery;
}

BatVoltage::~BatVoltage() {

}


