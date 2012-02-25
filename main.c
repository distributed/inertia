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


volatile uint16_t rt_hi;
#define SYNC_LOCK_INIT (30) // ~ 60 ms lockout
volatile uint8_t sync_locked;
 
ISR(TIM0_OVF_vect) {
    rt_hi++;
    if (sync_locked) sync_locked--;
}

ISR(INT0_vect) {
    // wake up from sleep
    GIMSK &= ~(1<<INT0); // disable INT0
}


volatile uint8_t locked;
volatile uint16_t nrad;
volatile uint8_t lastkey = (1<<PB2);

volatile uint32_t turntime;

ISR(TIM1_COMPA_vect) {
    if (locked) {
	output_column(nrad++);
    }

    uint8_t l_lastkey = lastkey;
    uint8_t key = PINB & (1<<PB2);
    if (!sync_locked) {
	if ((l_lastkey ^ key) && (!key)) {
	    // sync!
	    sync_locked = SYNC_LOCK_INIT;
	    nrad = 0;
	    turntime = 0;
	}
    }
    lastkey = key;

    turntime += TCNT1;
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



		
