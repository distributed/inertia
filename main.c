#include <stdint.h>
#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

#include "scroll.h"
#include "font_5x8.h"

#define SLEEP_INHIBIT (20)

uint8_t isr_attention;

scrollstate scroll;

void output_column(uint8_t col) {
    /*uint8_t ra = 0;
    for (int i = 0; i < 8; i++) {
	ra |= (col & 0x01);
	ra <<= 1;
	col >>= 1;
    }

    col = ra;*/

    PORTA &= ~(~(1<<PA4));
    PORTA |= col & ((1<<PA7) | 0x0f);
    PORTA |= (col & (1<<6)) >> 1; // bring bit 6 down to bit 5
    
    PORTB &= ~0x03;
    PORTB |= (col >> 4) & 0x03;
}


void ioinit() {
        DDRA = ~(1<<PA4);
        DDRB = (1<<PB1)|(1<<PB0);

        //PORTA = (1<<PA7);

        // turn on pullup for button
        PORTB |= (1<<PB2);
}



volatile uint8_t key_state;   // debounced and inverted key state:
                              // bit = 1: key pressed
volatile uint8_t key_press;   // key press detect
 
volatile uint8_t key_rpt;     // key long press and repeat

volatile uint8_t key_update;

 
ISR(TIM0_COMPA_vect)
{
    static uint8_t ct0, ct1; //, rpt;
  uint8_t i;

  uint16_t sret = scroll_step(&scroll);
  if (sret > 0) {
      //output column
      output_column((uint8_t) sret);
  }

  i = key_state ^ ~(((PINB & (1<<PB2)) | (~(1<<PB2))));                       // key changed ?
  ct0 = ~( ct0 & i );                             // reset or count ct0
  ct1 = ct0 ^ (ct1 & i);                          // reset or count ct1
  i &= ct0 & ct1;                                 // count until roll over ?
  key_state ^= i;                                 // then toggle debounced state
  key_press |= key_state & i;                     // 0->1: key press detect
 

  //output_column(key_state);
		    /*  if( (key_state & REPEAT_MASK) == 0 )            // check repeat function
     rpt = REPEAT_START;                          // start delay
  if( --rpt == 0 ){
    rpt = REPEAT_NEXT;                            // repeat delay
    key_rpt |= key_state & REPEAT_MASK;
    }*/

  key_update = 1;
  isr_attention = 1;
}

ISR(INT0_vect) {
    // wake up from sleep
    GIMSK &= ~(1<<INT0); // disable INT0
}

PROGMEM uint8_t msgs[] = "T\x81" "echli\0Z\x84pfli\0TCB\0RNR\0Fuck\0Yeah\0";

int main() {
    
    PRR = (1<<PRTIM1)|(1<<PRUSI)|(1<<PRADC); // disable timer1, usi, adc

    OCR0A = ((F_CPU * 25UL) / 10000UL) / 64UL; // see multiplicator, in
                                               // 1/10^4 ths of a second
    TCCR0A = (1<<WGM01)|(0<<WGM00); // CTC
    TCCR0B = (0<<CS02)|(1<<CS01)|(1<<CS00); // 1/64
    TIMSK0 = (1<<OCIE0A);

    ioinit();

    sei();
    
    output_column(0x0f);


    uint8_t msgcnt = 0;
    uint8_t *msgsp = msgs;
    uint8_t sleep_inhibit = SLEEP_INHIBIT;
    while (1) {
	if (key_update && (!scroll.active)) {
	    //if (scroll.active) continue;
	    if (sleep_inhibit) sleep_inhibit--;

	    if (key_press) {

		msgcnt++;
		if (msgcnt == 4) msgcnt = 0;

		scroll_init(&scroll, (uint8_t*) msgsp, (uint8_t*) font5x8, 5, SCROLL_FORWARD);
		
		msgsp += strlen_P(msgsp) + 1;
		if (!pgm_read_byte(msgsp)) {
		    msgsp = (uint8_t*) msgs;
		}
		
		scroll.active = 1;

		key_press = 0;
	    } else {
		output_column(0x00);
	    }
	    key_update = 0;
	}


	cli();
	if ((!isr_attention) && (!scroll.active) &&
	    (!key_state) && (!sleep_inhibit)) {
	    //if (sleep_inhibit) continue;



	    GIMSK |= (1<<INT0); // enable INT0, default is low level

	    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	    sleep_enable();
	    sei();
	    sleep_cpu();

	    sleep_disable();

	    //GIMSK &= ~(1<<INT0); // disable INT0

	    sleep_inhibit = SLEEP_INHIBIT;
	}
	sei();
	isr_attention = 0;
    }
}



		
