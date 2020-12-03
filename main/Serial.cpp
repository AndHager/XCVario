
#include <esp_log.h>
#include "BTSender.h"
#include <string>
#include "sdkconfig.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include <freertos/semphr.h>
#include <algorithm>
#include <HardwareSerial.h>
#include "RingBufCPP.h"
#include <driver/uart.h>
#include "OpenVario.h"
#include <logdef.h>
#include "Switch.h"
#include "sensor.h"
#include "Router.h"
#include "Serial.h"

/* Note that the standard NMEA 0183 baud rate is only 4.8 kBaud.
Nevertheless, a lot of NMEA-compatible devices can properly work with
higher transmission speeds, especially at 9.6 and 19.2 kBaud.
As any sentence can consist of 82 characters maximum with 10 bit each (including start and stop bit),
any sentence might take up to 171 ms (at 4.8k Baud), 85 ms (at 9.6 kBaud) or 43 ms (at 19.2 kBaud).
This limits the overall channel capacity to 5 sentences per second (at 4.8k Baud), 11 msg/s (at 9.6 kBaud) or 23 msg/s (at 19.2 kBaud).
If  too  many  sentences  are  produced  with  regard  to  the  available  transmission  speed,
some sentences might be lost or truncated.

 */



// #define FLARM_SIM
// Option to simulate FLARM sentences

#ifdef FLARM_SIM
char *flarm[] = {
		"$PFLAA,0,11461,-9272,1436,1,AFFFFF,51,0,257,0.0,0*7A\n",
		"$PFLAA,0,2784,3437,216,1,AFFFFE,141,0,77,0.0,0*56\n",
		"$PFLAA,1,-1375,1113,64,1,AFFFFD,231,0,30,0.0,0*43\n",
		"$PFLAA,0,-3158,3839,1390,1,A858F3,302,0,123,-12.4,0*6B\n",
		"$GPRMC,084854.40,A,44xx.xxx38,N,093xx.xxx15,W,0.0,0.0,300116,,,D*43\n",
		"$PFLAA,0,11621,-9071,1437,1,AFFFFF,52,0,257,0.0,0*7F\n",
		"$PFLAA,0,2724,3485,218,1,AFFFFE,142,0,77,0.0,0*58\n",
		"$PFLAA,1,-1394,1089,65,1,AFFFFD,232,0,30,0.0,0*4C\n",
		"$PFLAA,0,-3158,3839,1384,1,A858F3,302,0,123,-12.4,0*6E\n",
		"$GPRMC,084855.40,A,44xx.xxx38,N,093xx.xxx15,W,0.0,0.0,300116,,,D*42\n",
		"$GPGGA,152011.934,4850.555,N,00839.186,E,1,12,1.0,0.0,M,0.0,M,,*67\n",
		"$GPGSA,A,3,01,02,03,04,05,06,07,08,09,10,11,12,1.0,1.0,1.0*30\n",
		"$GPRMC,152949.571,A,4846.973,N,00843.677,E,,,220620,000.0,W*75\n"

};
#endif



// Serial Handler  ttyS1, S1, port 8881
void Serial::serialHandlerS1(void *pvParameters){

#ifdef FLARM_SIM
	int i=0;
#endif
	while(1) {

#ifdef FLARM_SIM
		portENTER_CRITICAL_ISR(&btmux);
		bt_tx_q.add( flarm[i++] );
		portEXIT_CRITICAL_ISR(&btmux);
		// ESP_LOGD(FNAME,"Serial TX: %s",  flarm[i-1] );
		if(i>12)
			i=0;
#endif

		SString s;
		// Serial Interface tty1 send
		if ( !s1_tx_q.isEmpty() && Serial1.availableForWrite() ){
			ESP_LOGD(FNAME,"Serial Data and avail");
			while( pullMsg( s1_tx_q, s ) ) {
				ESP_LOGD(FNAME,"Serial 1 TX len: %d bytes", s.length() );
				ESP_LOG_BUFFER_HEXDUMP(FNAME,s.c_str(),s.length(), ESP_LOG_DEBUG);
				int wr = Serial1.write( s.c_str(), s.length() );
				ESP_LOGD(FNAME,"Serial 1 TX written: %d", wr);
			}
		}
		int num = Serial1.available();
		if( num > 0 ) {
			// ESP_LOGI(FNAME,"Serial 1 RX avail %d bytes", num );
			if( num >= SSTRLEN ) {
				ESP_LOGW(FNAME,"Serial 1 Overrun RX >= %d bytes avail: %d, Bytes", SSTRLEN, num);
				num=SSTRLEN;
			}
			int numread = Serial1.read( s.c_str(), num );
			ESP_LOGD(FNAME,"Serial 1 RX bytes read: %d", numread );
			// ESP_LOG_BUFFER_HEXDUMP(FNAME,rxBuffer,numread, ESP_LOG_INFO);
			s.setLen( numread );
			forwardMsg( s, s1_rx_q );
		}
		routeS1();
		routeBT();
		routeWLAN();
	    BTSender::transmit(s);   // piggyback this here, saves one task for BT sender, SString s is recycled as buffer
		esp_task_wdt_reset();
		vTaskDelay( HEARTBEAT_PERIOD_MS/portTICK_PERIOD_MS );
	}
}

// ttyS2, port 8882
void Serial::serialHandlerS2(void *pvParameters){
	while(1) {
		routeBT();
		SString s;
		if ( !s2_tx_q.isEmpty() && Serial2.availableForWrite() ){
			if( pullMsg( s2_tx_q, s ) ) {
				ESP_LOGD(FNAME,"Serial Data and avail");
				ESP_LOGD(FNAME,"Serial 2 TX len: %d bytes", s.length() );
				ESP_LOG_BUFFER_HEXDUMP(FNAME,s.c_str(),s.length(), ESP_LOG_DEBUG);
				int wr = Serial2.write( s.c_str(), s.length() );
				ESP_LOGD(FNAME,"Serial 2 TX written: %d", wr);
			}
		}
		int num = Serial2.available();
		if( num > 0 ) {
			// ESP_LOGD(FNAME,"Serial 2 RX avail %d bytes", num );
			if( num >= SSTRLEN ) {
				ESP_LOGW(FNAME,"Serial 2 RX Overrun >= %d bytes avail: %d, Bytes", SSTRLEN, num);
				num=SSTRLEN;
			}
			int numread = Serial2.read( s.c_str(), num );
			ESP_LOGD(FNAME,"Serial 2 RX bytes read: %d", numread );
			// ESP_LOG_BUFFER_HEXDUMP(FNAME,rxBuffer,numread, ESP_LOG_INFO);
			s.setLen( numread );
			forwardMsg( s, s2_rx_q );
		}
		routeWLAN();
		routeS2();
		esp_task_wdt_reset();
		vTaskDelay( HEARTBEAT_PERIOD_MS/portTICK_PERIOD_MS );  // 48 bytes each 20 mS traffic at 19.200 baud
	}
}


void Serial::begin(){
	ESP_LOGI(FNAME,"Serial::begin()" );
	if( serial1_speed.get() != 0  || blue_enable.get() ){
		int baudrate = baud[serial1_speed.get()];
		if( baudrate != 0 ) {
			ESP_LOGI(FNAME,"Serial Interface ttyS1 enabled with serial speed: %d baud: %d tx_inv: %d rx_inv: %d",  serial1_speed.get(), baud[serial1_speed.get()], serial1_tx_inverted.get(), serial1_rx_inverted.get() );
			Serial1.begin(baudrate,SERIAL_8N1,16,17, serial1_rx_inverted.get(), serial1_tx_inverted.get());   //  IO16: RXD2,  IO17: TXD2
			Serial1.setRxBufferSize(256);
		}
		// need this for bluetooth
		xTaskCreatePinnedToCore(&Serial::serialHandlerS1, "serialHandlerS1", 4096, NULL, 27, 0, 0);
	}
	if( serial2_speed.get() != 0  && hardwareRevision.get() >= 3 ){
		ESP_LOGI(FNAME,"Serial Interface ttyS2 enabled with serial speed: %d baud: %d tx_inv: %d rx_inv: %d",  serial2_speed.get(), baud[serial2_speed.get()], serial2_tx_inverted.get(), serial2_rx_inverted.get() );
		if( serial2_pins_twisted.get() )  //   speed, RX, TX, invRX, invTX
			Serial2.begin(baud[serial2_speed.get()],SERIAL_8N1,4,18, serial2_rx_inverted.get(), serial2_tx_inverted.get());   //  IO16: RXD2,  IO17: TXD2
		else
			Serial2.begin(baud[serial2_speed.get()],SERIAL_8N1,18,4, serial2_rx_inverted.get(), serial2_tx_inverted.get());   //  IO16: RXD2,  IO17: TXD2

		Serial2.setRxBufferSize(256);
		xTaskCreatePinnedToCore(&Serial::serialHandlerS2, "serialHandlerS2", 4096, NULL, 26, 0, 0);
	}
}


