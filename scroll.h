#ifndef SCROLL_H
#define SCROLL_H

typedef struct {
    uint8_t active;
    
    uint8_t *font;
    uint8_t fontwidth;
    
    uint8_t *string;
    uint8_t len;
  
    uint8_t increment;
  
    uint8_t bitcnt;
    uint8_t bytecnt;
} scrollstate;



#define SCROLL_FORWARD 0
#define SCROLL_BACKWARD 1


#define SCROLL_FONTWIDTH(s) (5)
//#define SCROLL_FONTWIDTH(s) (s->fontwidth)


void scroll_init(volatile scrollstate *s, uint8_t *string, uint8_t *font, uint8_t fontwidth, uint8_t dir);
uint16_t scroll_step(volatile scrollstate *s);

#endif
