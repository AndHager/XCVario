#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp32_ili9341.h"
#include "SPI.h"
#include "driver/gpio.h"

extern "C" {

static esp32_hal_config_t *config;

static void einit(eglib_t *eglib) {
	config = (esp32_hal_config_t *)eglib_GetHalConfig(eglib);
	SPI.begin( config->gpio_scl, config->gpio_sdi, config->gpio_sda, config->gpio_cs );  // done already by sensors and CS handled in here
	// init GPIO pins of 4 WIRE SPI bus
	gpio_set_direction(config->gpio_sdi, GPIO_MODE_INPUT);
	gpio_set_direction(config->gpio_sda, GPIO_MODE_OUTPUT);
	gpio_set_direction(config->gpio_scl, GPIO_MODE_OUTPUT);
	gpio_set_direction(config->gpio_cs, GPIO_MODE_OUTPUT);
	gpio_set_direction(config->gpio_rs, GPIO_MODE_OUTPUT);
	gpio_set_direction(config->gpio_dc, GPIO_MODE_OUTPUT);

	// hal_four_wire_spi_config_t * fwc = eglib_GetHalFourWireSpiConfigComm(eglib);
	// SPI.setClockDivider( SPI_CLOCK_DIV8 );
}

static void esleep_in(eglib_t *_eglib) {
	// we are not using this, ESP32 SPI sleep is not helpful for XCVario
}

static void esleep_out(eglib_t *_eglib) {
	// dito
}

static void edelay_ns(eglib_t *_eglib, uint32_t ns) {
	vTaskDelay( (ns/1000000) / portTICK_PERIOD_MS);
}

static void eset_reset(eglib_t *_eglib, bool state) {
	gpio_set_level(config->gpio_rs, (unsigned int)state );
}

static bool eget_busy(eglib_t *_eglib) {
  return false;
}

static void ecomm_begin(eglib_t *_eglib) {
	config = (esp32_hal_config_t *)eglib_GetHalConfig(_eglib);
	gpio_set_level(config->gpio_cs, 0 );
	SPI.beginTransaction(SPISettings( config->freq, config->bitOrder, config->dataMode ));  // *3
}

static void egsend(
	eglib_t *_eglib,
	// enum hal_dc_t dc,
	uint32_t dc,
	uint8_t *bytes,
	uint32_t length )
{
	config = (esp32_hal_config_t *)eglib_GetHalConfig(_eglib);
	gpio_set_level(config->gpio_dc, dc );
	while( length ){
		SPI.transfer( *bytes );
		bytes++;
	}
}

static void ecomm_end(eglib_t *_eglib) {
	config = (esp32_hal_config_t *)eglib_GetHalConfig(_eglib);
	SPI.endTransaction();
	gpio_set_level(config->gpio_cs, 1 );
}

hal_t esp32_ili9341 = {
	.init = einit,
	.sleep_in = esleep_in,
	.sleep_out = esleep_out,
	.delay_ns = edelay_ns,
	.set_reset = eset_reset,
	.get_busy = eget_busy,
	.comm_begin = ecomm_begin,
	.send = egsend,
	.comm_end = ecomm_end,
};
}
