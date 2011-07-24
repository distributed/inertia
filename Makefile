COBJS=main.o scroll.o font_5x8.o
ASMOBJS=

CCARCH=attiny44
ADARCH=attiny44
F_CPU=1000000UL
PORT=usb

CC=avr-gcc
OBJCOPY=avr-objcopy
AVRDUDE=avrdude

CFLAGS=-Os
CFLAGS+=-mmcu=${CCARCH}
CFLAGS+=-gstabs
CFLAGS+=-DF_CPU=${F_CPU}
CFLAGS+=-std=c99
CFLAGS+=-Wall
#CFLAGS+=-mcall-prologues

ASMCFLAGS=-mmcu=${CCARCH}
ASMCFLAGS+=-DF_CPU=${F_CPU}
ASMCFLAGS+=-Wall

AVRDUDE_PROGRAMMER=avrisp2
AVRDUDE_OPTS=


LDFLAGS=-mmcu=${CCARCH} -gstabs


hex: main
	${OBJCOPY} -j .text -j .data -O ihex main main.hex

main: ${COBJS} ${ASMOBJS}
	${CC} -o main ${LDFLAGS} ${COBJS} ${ASMOBJS}

.c.o:
	${CC} ${CFLAGS} -c $<

.S.o:
	${CC} ${ASMCFLAGS} -c $<


clean:
	rm -rf *.o main *.hex pcreader

dump: main
	avr-objdump -S main

burn: hex
	${AVRDUDE} -p ${ADARCH} -c ${AVRDUDE_PROGRAMMER} ${AVRDUDE_OPTS} -P ${PORT} -U flash:w:main.hex:i

pc:
	gcc -o pcreader -DPCREADER pcreader.c bitstream.c amx.c compact.c

avrdudeline:
	echo ${AVRDUDE} -p ${ADARCH} -c ${AVRDUDE_PROGRAMMER} ${AVRDUDE_OPTS} -P ${PORT}