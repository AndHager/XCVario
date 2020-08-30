#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "u8g2_esp32_hal.h"
#include <SPI.h>
#include <esp32-hal-spi.h>
#include <Arduino.h>
#include <logdef.h>

static SPIClass *_Spi; // (HSPI);
#define FREQUENCY 40000000

static SPISettings spis( FREQUENCY, SPI_MSBFIRST, SPI_MODE0 );


static u8g2_esp32_hal_t    u8g2_esp32_hal;  // HAL state data.

#undef ESP_ERROR_CHECK
#define ESP_ERROR_CHECK(x)   do { esp_err_t rc = (x); if (rc != ESP_OK) { ESP_LOGE("err", "esp_err_t = %d", rc); assert(0 && #x);} } while(0);

/*
 * Initialze the ESP32 HAL.
 */
void u8g2_esp32_hal_init(u8g2_esp32_hal_t u8g2_esp32_hal_param) {
	ESP_LOGI(FNAME,"HAL init  ******************");
	u8g2_esp32_hal = u8g2_esp32_hal_param;
} // u8g2_esp32_hal_init

/*
 * HAL callback function as prescribed by the U8G2 library.  This callback is invoked
 * to handle SPI communications.
 */
uint8_t u8g2_esp32_spi_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
	// ESP_LOGI(FNAME,"HAL: spi_byte_cb: Received a msg: %d", msg ); // arg_int: %d, arg_ptr: %p\n", msg, arg_int, arg_ptr);
	switch(msg) {

	case U8X8_MSG_CAD_END_TRANSFER:
	{
		// ESP_LOGI(FNAME,"U8X8_MSG_CAD_END_TRANSFER");
	}
	break;

	case U8X8_MSG_BYTE_SET_DC:
	{
		// ESP_LOGI(FNAME,"HAL: Set level D/C: %d", arg_int );
		digitalWrite(u8g2_esp32_hal.dc, arg_int);
	}
	break;

	case U8X8_MSG_BYTE_INIT:
	{
		ESP_LOGI(FNAME,"U8X8_MSG_BYTE_INIT: Init SPI");
		_Spi = new SPIClass(HSPI);
		_Spi->begin(u8g2_esp32_hal.clk, u8g2_esp32_hal.miso, u8g2_esp32_hal.mosi, u8g2_esp32_hal.cs );
		_Spi->setHwCs( true );
		_Spi->setFrequency(FREQUENCY);
		pinMode( u8g2_esp32_hal.cs, GPIO_MODE_OUTPUT);
		pinMode( u8g2_esp32_hal.reset, GPIO_MODE_OUTPUT);
		pinMode( u8g2_esp32_hal.dc, GPIO_MODE_OUTPUT);
		// pinMode( u8g2_esp32_hal.mosi, GPIO_MODE_OUTPUT);
		uint64_t bitmask = 0;
	  	bitmask = bitmask | (1<<u8g2_esp32_hal.dc);
		bitmask = bitmask | (1<<u8g2_esp32_hal.reset);
		bitmask = bitmask | (1<<u8g2_esp32_hal.cs);
		gpio_config_t gpioConfig;
		gpioConfig.pin_bit_mask = bitmask;
		gpioConfig.mode         = GPIO_MODE_OUTPUT;
		gpioConfig.pull_up_en   = GPIO_PULLUP_DISABLE;
		gpioConfig.pull_down_en = GPIO_PULLDOWN_ENABLE;
		gpioConfig.intr_type    = GPIO_INTR_DISABLE;
		gpio_config(&gpioConfig);
	}
	break;

	case U8X8_MSG_BYTE_SEND:
	{
		// ESP_LOGI(FNAME,"U8X8_MSG_BYTE_SEND len:%d CS:%d", arg_int, u8g2_esp32_hal.cs);
		_Spi->beginTransaction( spis );
		digitalWrite(u8g2_esp32_hal.cs, LOW);
		_Spi->transferBytes( (uint8_t *)arg_ptr, 0, arg_int );
		digitalWrite(u8g2_esp32_hal.cs, HIGH);
		_Spi->endTransaction();
	}
	break;

	}
	return 0;
} // u8g2_esp32_spi_byte_cb

/*
 * HAL callback function as prescribed by the U8G2 library.  This callback is invoked
 * to handle I2C communications.
 */
uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
	// ESP_LOGD(TAG, "i2c_cb: Received a msg: %d, arg_int: %d, arg_ptr: %p", msg, arg_int, arg_ptr);
    return 0;
} // u8g2_esp32_i2c_byte_cb

/*
 * HAL callback function as prescribed by the U8G2 library.  This callback is invoked
 * to handle callbacks for GPIO and delay functions.
 */
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
	// ESP_LOGI(FNAME,"gpio_and_delay_cb: Received a msg  %d", msg );

	switch(msg) {
	// Initialize the GPIO and DELAY HAL functions.  If the pins for DC and RESET have been
	// specified then we define those pins as GPIO outputs.
	case U8X8_MSG_GPIO_AND_DELAY_INIT: {
		ESP_LOGI(FNAME,"HAL: Init GPIO dc:%d res:%d cs:%d", u8g2_esp32_hal.dc,u8g2_esp32_hal.reset,u8g2_esp32_hal.cs );
		break;
	}
	// Set the GPIO reset pin to the value passed in through arg_int.
	case U8X8_MSG_GPIO_RESET:
		ESP_LOGI(FNAME,"HAL: Set level RESET: %d", arg_int );
		digitalWrite(u8g2_esp32_hal.reset, arg_int);
		break;
		// Set the GPIO client select pin to the value passed in through arg_int.
	case U8X8_MSG_GPIO_CS:
		ESP_LOGI(FNAME,"HAL: Set level CS: %d", arg_int );
		digitalWrite(u8g2_esp32_hal.cs, arg_int);
		break;
		// Set the Software I²C pin to the value passed in through arg_int.
	case U8X8_MSG_GPIO_I2C_CLOCK:
		if (u8g2_esp32_hal.scl != U8G2_ESP32_HAL_UNDEFINED) {
			// ESP_LOGI(FNAME,"Set level I2C SCL: %d", arg_int );
			// gpio_set_level(u8g2_esp32_hal.scl, arg_int);
		}
		break;
		// Set the Software I²C pin to the value passed in through arg_int.
	case U8X8_MSG_GPIO_I2C_DATA:
		if (u8g2_esp32_hal.sda != U8G2_ESP32_HAL_UNDEFINED) {
			// ESP_LOGI(FNAME,"Set level I2C SCL: %d", arg_int );
			// gpio_set_level(u8g2_esp32_hal.sda, arg_int);
		}
		break;

		// Delay for the number of milliseconds passed in through arg_int.
	case U8X8_MSG_DELAY_MILLI:
		ESP_LOGI(FNAME,"HAL: Delay: %d", arg_int );
		vTaskDelay(arg_int/portTICK_PERIOD_MS);
		break;
	}
	return 0;
} // u8g2_esp32_gpio_and_delay_cb



