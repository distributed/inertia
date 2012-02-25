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

/* 
  
  Hardware connections:
  PA0		-		LED0 (anode, cathode to ground)
  PA1		-		LED1 (anode, cathode to ground)
  PA2		-		LED2 (anode, cathode to ground)
  PA3		-		LED3 (anode, cathode to ground)
  PB0		-		LED4 (anode, cathode to ground)
  PB1		-		LED5 (anode, cathode to ground)
  PA5		-		LED6 (anode, cathode to ground)
  PA7		-		LED7 (anode)
  PA6		-		LED7 (cathode)
  PB2		-		Push button/reed switch, internally pulled-up
  
  LED0 is at the top, LED7 is at the bottom. All the
  standard ISP pins should be connected.
  
  Timer 0 keeps track of "real time" and handles some time outs
  for the radial display routine. The synchronization with the
  wheel and the radial display are handled in the Timer 1 Comp
  A ISR. The Compare Value A is changed to reflect the deduced
  wheel speed.


 */


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

// the high 16 bits of the "real time" counter
volatile uint16_t rt_hi;

#define SYNC_LOCK_INIT (30) // ~ 60 ms lockout
#define SYNC_SEARCH_INIT (1000)
volatile uint8_t sync_lockout; // if > 0, no sync pulse is accepted
volatile uint16_t sync_search; // if > 0, we're in a valid speed range to
                               // accept the next sync pulse
 
ISR(TIM0_OVF_vect) {
    rt_hi++;
    if (sync_lockout) sync_lockout--;
    if (sync_search) sync_search--;
}

ISR(INT0_vect) {
    // wake up from sleep
    GIMSK &= ~(1<<INT0); // disable INT0
}


volatile uint8_t locked; // is the wheel speed known?
volatile uint16_t nrad; // current number of radial
volatile uint8_t lastkey = (1<<PB2); // last key state

volatile uint32_t turntime; // time accumulator since last sync pulse

ISR(TIM1_COMPA_vect) {
    if (locked) {
	output_column(nrad++);
	if (!sync_search) {
	    locked = 0;
	}
    } else {
	output_column(0x00);
    }

    // note: turntime is incorrectly updated with turntime, turntime
    // is incorrectly updated through turntime: there is no +/- 1
    // compensation because OCR1A and radial time are off by 1. the
    // fractional part of OCR1A is lost.

    uint16_t l_ocr1a = OCR1A;
    uint8_t l_lastkey = lastkey;
    uint8_t key = PINB & (1<<PB2);
    if (!sync_lockout) {
	if ((l_lastkey ^ key) && (!key)) {
	    // sync pulse
	    if (sync_search) {
		// turntime ready for OCRA calculation
		nrad = 0; // sync pulse retriggers sequence
		locked = 1;

		uint16_t ocra = turntime / 256UL;
		if ((ocra > (500/8))) {
		    OCR1A = ocra; // TODO: compensate by -1
		} else {
		    locked = 0;
		}
	    }
	    sync_search = SYNC_SEARCH_INIT;
	    sync_lockout = SYNC_LOCK_INIT;
	    turntime = 0;
	}
    }
    lastkey = key;

    turntime += l_ocr1a; // TODO: compensate by +1
}

int main() {
    
    PRR = (0<<PRTIM1)|(1<<PRUSI)|(1<<PRADC); // disable timer1, usi, adc

    TCCR0A = 0; // normal
    TCCR0B = (0<<CS02)|(1<<CS01)|(0<<CS00); // 1/8
    TIMSK0 = (1<<TOIE0); // timer triggers ~ 490 times/s. cycle time ~ 2 ms

    //             ps   #rads #rots/s
    OCR1A = F_CPU / 8 / 256 / 2;
    TCCR1A  = (1<<WGM11)|(1<<WGM10);
    //        fast PWM to OCR1A                    1/8
    TCCR1B  = (1<<WGM13)|(1<<WGM12) | (0<<CS12)|(1<<CS11)|(0<<CS10); 
    TIMSK1 = (1<<OCIE1A);
    

    ioinit();

    sei();
    
    output_column(0x0f);
    _delay_ms(50);
    output_column(0x00);

    while (1) {
	cli();
	set_sleep_mode(SLEEP_MODE_IDLE);
	sleep_enable();
	sei();
	sleep_cpu();

	sleep_disable();
    }
}



		
