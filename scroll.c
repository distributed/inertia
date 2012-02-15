#include <stdint.h>
#include <avr/pgmspace.h>
#include "scroll.h"


void scroll_init(volatile scrollstate *s, uint8_t *string, uint8_t *font, uint8_t fontwidth, uint8_t dir) {
    s->string = string;
    s->len = strlen_P((PGM_P) string);
    s->font = font;
    if (dir == SCROLL_FORWARD) {
	s->bitcnt = 0;
	s->bytecnt = 0;
	s->increment = 1;
    } else {
	s->bitcnt = 7;  // TODO: SCROLL_FONTWIDTH(s)-1 ?
	s->bytecnt = s->len-1;
	s->increment = (uint8_t) -1;
    }
}


uint16_t scroll_step(volatile scrollstate *s) {
    if (!s->active) {
	return 0;
    }

    // read character
    uint8_t ch = pgm_read_byte(&(s->string[s->bytecnt]));

    //uint16_t boffset = SCROLL_FONTWIDTH(s)*ch;
    // for fontwidth == 5:  // TODO: find smart way to encode mult without mult
    // for the general case
    uint16_t boffset = (ch << 2) + ch;
    uint8_t col = pgm_read_byte(s->font + (boffset) + s->bitcnt);

    s->bitcnt += s->increment;
    // TODO: >= even for unsigned type and increment -1?
    // it works because 0-1 == 255, but it's a bit ugly and at
    // least worth a comment
    if (s->bitcnt >= SCROLL_FONTWIDTH(s)) {

	if (s->increment == 1) {
	    s->bitcnt = 0;
	} else {
	    s->bitcnt = SCROLL_FONTWIDTH(s)-1;
	}

	s->bytecnt += s->increment;
	if (s->bytecnt >= s->len) {
	    s->active = 0;
	    return col; // terminates early in case of all-off column
	}
    }
    return (1<<8) | col;
}
