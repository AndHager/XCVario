#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include <freertos/queue.h>
#include "freertos/task.h"
#include "string.h"
#include "esp_system.h"
#include "ESPRotary.h"
#include <esp32/rom/ets_sys.h>
#include <sys/time.h>
#include <Arduino.h>
#include "RingBufCPP.h"


gpio_num_t ESPRotary::clk, ESPRotary::dt, ESPRotary::sw;
std::vector<RotaryObserver *> ESPRotary::observers;
uint8_t ESPRotary::_switch;
QueueHandle_t ESPRotary::q2;
TickType_t ESPRotary::xLastWakeTime;
ring_buffer ESPRotary::rb(2);
int ESPRotary::dir=0;
int ESPRotary::last_dir=0;
int ESPRotary::last=0;
int ESPRotary::_switch_state=1;
int ESPRotary::n=0;
long ESPRotary::lastmilli=0;
int ESPRotary::errors=0;
uint64_t ESPRotary::swtime=0;
SemaphoreHandle_t ESPRotary::xBinarySemaphore;
TaskHandle_t xHandle;
RingBufCPP<t_rot, 10> rb1;

void ESPRotary::attach(RotaryObserver *obs) {
	observers.push_back(obs);
}

ESPRotary::ESPRotary() {
	swMutex = xSemaphoreCreateMutex();
}

void ESPRotary::begin(gpio_num_t aclk, gpio_num_t adt, gpio_num_t asw ) {
	clk = aclk;
	dt = adt;
	sw = asw;
	gpio_config_t gpioConfig;
	memset(&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.pin_bit_mask |= (1 << clk);
	gpioConfig.pin_bit_mask |= (1 << dt);
	gpioConfig.pin_bit_mask |= (1 << sw);
	gpioConfig.mode = GPIO_MODE_INPUT;
	gpioConfig.pull_up_en = GPIO_PULLUP_ENABLE;
	gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
	gpioConfig.intr_type = GPIO_INTR_ANYEDGE;
	gpio_config(&gpioConfig);
	gpio_install_isr_service(0);
	// q1 = xQueueCreate(8, sizeof(struct _rotbyte) );
	q2 = xQueueCreate(8, sizeof(enum _event));
	rb.push( 0 );
	rb.push( 0 );
	gpio_isr_handler_add(clk, ESPRotary::readPosInt, NULL);
	gpio_isr_handler_add(sw, ESPRotary::readPosInt, NULL);
	xTaskCreatePinnedToCore(&ESPRotary::readPos, "readPos", 1024*4, NULL, 30, &xHandle, 0);
	xTaskCreatePinnedToCore(&ESPRotary::informObservers, "informObservers", 1024*8, NULL, 29, NULL, 0);
	xBinarySemaphore = xSemaphoreCreateBinary();
}

enum _event infoalt = NONE;
int events[ MAX_EVENT ];

void ESPRotary::informObservers( void * args )
{
	enum _event info;
	for( int i=0; i < MAX_EVENT; i++ )
		events[i] = 0;
	int ialt = NONE;
	while( 1 ) {
		int queue = xQueueGenericReceive(q2, &info, portMAX_DELAY, false );
		// printf("Queue Poll: %d  info:%d\n", queue, info );
		// printf("Queue: %d \n", queue );
		ialt = info;
		events[info]++;
		int runs = 8;
		int mq = uxQueueMessagesWaiting(q2);
		while( mq && runs ) {
			xQueueGenericReceive(q2, &info, portMAX_DELAY, false);
			// printf("Messages in the Queue: %d  info:%d  run:%d\n", mq, info, runs );
			if( queue )
				events[info]++;
			if( ialt != info ){
				ialt = info;
				break;
			}
			sleep(0.05);
			runs --;
			mq = uxQueueMessagesWaiting(q2);
		}

		for( int i=0; i < MAX_EVENT; i++ )
		{
			int num;
			if(  events[i] == 0 )
				continue;
			num = events[i];
			// printf("Event i:%d number events:%d \n", i, events[i] );
			if( i == UP ) {
				printf("Rotary up %d times\n", num );
				for (int i = 0; i < observers.size(); i++)
					observers[i]->up( num );
			}
			else if( i == DOWN ) {
				printf("Rotary down %d times\n", num);
				for (int i = 0; i < observers.size(); i++)
					observers[i]->down( num );
			}
			else if( i == RELEASE ){      // pullup, so not pressed is 1
				printf("Switch released action\n");
				for (int i = 0; i < observers.size(); i++)
					observers[i]->release();
				printf("End Switch released action\n");
			}
			else if ( i == PRESS ){
				printf("Switch pressed action\n");
				for (int i = 0; i < observers.size(); i++)
					observers[i]->press();
				printf("End Switch pressed action\n");
			}
			else if ( i == LONG_PRESS ){
				printf("Switch long pressed action\n");
				for (int i = 0; i < observers.size(); i++)
					observers[i]->longPress();
				printf("End Switch long pressed action\n");
			}
			else if ( i == ERROR ){
				printf("ERROR ROTARY SEQ %d\n", errors++ );
			}
		}
		for( int i=0; i < MAX_EVENT; i++ )
			events[i] = 0;
	}
}

// receiving from Interrupt the rotary direction and switch
void ESPRotary::readPos(void * args) {

	while( 1 ){
		while( !rb1.isEmpty() )
		{
			t_rot rotary;
			enum _event var;
			n++;
			rb1.pull( &rotary );
			rb.push( rotary.rot & 3 );
			int r0=rb[0]&3;
			int r1=rb[1]&3;

			if( r1 != r0 )
			{
				// printf( "Rotaty dt/clk cur:%d last:%d\n", r0, r1 );
				if( (r0 == 0) && (r1 == 3) ) {
					dir--;
				}
				else if ( (r0 == 1) && (r1 == 2) ) {
					dir++;
				}
				else if( (r0 == 3) && (r1 == 0) ) {
					// printf("Rotary 30\n");
					// ignore
				}
				else if ( (r0 == 2) && (r1 == 1) ) {
					// printf("Rotary 21\n");
					// ignore
				}
				else if ( (r0 == 3) && (r1 == 1) ) {
					// printf("Rotary 31\n");
					// ignore
				}
				else {
					var = ERROR;
					printf( "Rotary seq incorrect cur:%d last:%d\n", r0, r1 );
				}
				int delta = dir - last_dir;

				if( delta < 0 ) {
					// printf("Rotary up\n");
					var = UP;
					xQueueSend(q2, &var, portMAX_DELAY );
				}
				if( delta  > 0 ) {
					// printf("Rotary down\n");
					var = DOWN;
					xQueueSend(q2, &var, portMAX_DELAY );
				}
				last_dir = dir;
			}

			int newsw = 0;
			if( rotary.rot & 0x04 )
				newsw = 1;
			if( newsw != _switch_state ){
				printf("Switch state changed\n");
				// Switch changed
				if( newsw == 1 ){
					int duration = rotary.time - swtime;
					printf("Switch released time since press=%d\n",duration);
					// switch now 1 == RELEASED
					if( (swtime != 0) && (duration > 5000) ){
						printf("Switch timeout for long press\n");
						// okay, last stored time is longer than long press timeout, we also
						// trigger now long press
						var = LONG_PRESS;
						xQueueSend(q2, &var, portMAX_DELAY );
						swtime = 0;
					}
					// In any case, switch has been released, send release
					var = RELEASE;
					xQueueSend(q2, &var, portMAX_DELAY );
				}
				else{
					printf("Switch pressed\n");
					// switch now 0 == PRESSED
					swtime = rotary.time;
					var = PRESS;
					xQueueSend(q2, &var, portMAX_DELAY );
				}
				_switch_state = newsw;
			}
		}
		delay(5);
		vTaskSuspend( NULL );
	}
}

#define NUM_LOOPS 50


void ESPRotary::readPosInt(void * args) {
	struct _rotbyte var;
	var.rot = 0;
	var.time = esp_timer_get_time()/1000;
	int dtv=0;
	int swv=0;
	int clkv=0;
	// debounce//
	for( int i=0; i<NUM_LOOPS; i++ ) {
		ets_delay_us(10);
		if (gpio_get_level(dt))
			dtv++;
		if (gpio_get_level(sw))
			swv++;
		if (gpio_get_level(clk))
			clkv++;
	}
	if( dtv > NUM_LOOPS/2 )
		var.rot |= 1;
	if( clkv > NUM_LOOPS/2 )
		var.rot |= 2;
	if( swv > NUM_LOOPS/2 )
		var.rot |= 4;

	rb1.add( var );

	// Resume the suspended task.
	BaseType_t xYieldRequired = xTaskResumeFromISR( xHandle );
	if( xYieldRequired == pdTRUE )
		portYIELD_FROM_ISR();
}

/////////////////////////////////////////////////////////////////
