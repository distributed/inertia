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
  Intended Target: An ATiny44 microcntroller equipped with
  a column of 8 LEDs for use as a POV device. A 
  button/reed switch controls when a new text is to be drawn.
  

  Operation: At the push of a button connected to PB2 (INT0), the microcontroller
  lights up its column of 8 LEDs according to the message selected in the main
  loop. If a message is being flashed out, but there's no work to do in the
  main loop, the controller enters a shallow sleep mode. If the main loop's
  out of work and the controller is not currently flashing a message, the
  controller goes into a deep sleep mode.
  
  
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
  
  
  While the controller is not in deep sleep mode, the system clock
  is running and feeding Timer 0. Timer 0 is set up to generate an
  Output Compare Match A interrupt every so often. (2.5 ms when the
  device is supposed to be moved manually) The interrupt routine
  calls scroll_step to deduce which LEDs to light up in the current
  position in the text. A return value of 0 means that the LEDs
  should be off. The LEDs are set using output_column which
  demangles logical LED numbers to the actual pin locations.
  The scroll state machine turns itself off automatically at
  the end of the message.
  After the scroll logic, the ISR debounces the input keys. Every way through
  the ISR, key_update is set to 1 to indicate new valid key state.
  The ISR also sets isr_attention to 1 in order to signal to the
  main loop that it needs to be run completely at least one more
  time. isr_attention != 0 prevents the controller from going to
  sleep, preventing sleep when there's pending data from interrupts.
  
  The main loop checks for the key being pressed. If an edge
  leading from unpressed -> depressed is detected, the main
  loop sets up the scroll state with a new message to display.
  This is done with a call to scroll_step(). The scroll state
  machine is then activated by setting .active to 1. The controller
  starts displaying the message the next time the TIMER0_COMPA 
  is called. Messages are selected consecutively out of the
  constant buffer msgs. It contains a number of '\0' terminated
  strings, its end is marked by a double '\0'.
  
  At the end of the main loop, the shallow and deep sleep modes
  are manages. The controller may enter shallow sleep mode
  during the display of a message or some time (set by SLEEP_INHIBIT)
  after. If the main loop has been run completely one time
  with isr_attention == 0, no stimuli are present and the controller
  may go to deep sleep. The controller can only be woken up from
  deep by generating an INT0 interrupt. The INT0 interrupt routine
  is almost a stub - it only clears its own interrupt enable to
  prevent the controller from being overwhelmed by a large
  number of interrupts generated to key bounce.

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


// debounce routines by Peter Danegger
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

PROGMEM uint8_t msgs[] = "T\x81" "echli\0Z\x84pfli\0Ficken Geil\0Fuck\0Yeah\0";
//PROGMEM uint8_t msgs[] = "Em Dani sini Muetter\0esch mega FETT!\0";

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
	    // if there's _nothing_ to do and nothing going on,
	    // go to deep sleep

	    //if (sleep_inhibit) continue;



	    GIMSK |= (1<<INT0); // enable INT0, default is low level

	    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	    sleep_enable();
	    sei();
	    sleep_cpu();

	    sleep_disable();

	    //GIMSK &= ~(1<<INT0); // disable INT0

	    sleep_inhibit = SLEEP_INHIBIT;
	} else if (!isr_attention) {
	    // if there's nothing to do, but stuff going on,
	    // enter shallow sleep

	    set_sleep_mode(SLEEP_MODE_IDLE);
	    sleep_enable();
	    sei();
	    sleep_cpu();

	    sleep_disable();
	}
	sei();
	isr_attention = 0;
    }
}



		
