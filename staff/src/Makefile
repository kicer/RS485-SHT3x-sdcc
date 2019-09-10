## Select one of these
#DEVICE=stm8s003f3
DEVICE=stm8s103f3

## A directory for common include files
COMMONDIR = bsp

## Get program name from enclosing directory name
PROGRAM = $(lastword $(subst /, ,$(CURDIR)))

SOURCES=$(wildcard *.c $(COMMONDIR)/*.c)
OBJECTS=$(SOURCES:.c=.rel)
HEADERS=$(wildcard *.h $(COMMONDIR)/*.h)

CC = /Developer/sdcc/bin/sdcc
PROGRAMMER = stlinkv2
FLASHER = /Developer/sdcc/bin/stm8flash
FACTORYOPT = /Developer/sdcc/stm8s103_opt_factory_defaults.bin


DEFINES=
## Set MCU-type DEFINE
ifeq ($(DEVICE),stm8s003f3)
    DEFINES += -DSTM8S003
endif
ifeq ($(DEVICE),stm8s103f3)
    DEFINES += -DSTM8S103
endif

CPPFLAGS = -I$(COMMONDIR)
CFLAGS = --Werror --std-sdcc99 -mstm8 $(DEFINES)
LDFLAGS = -lstm8 -mstm8 --out-fmt-ihx

.PHONY: all clean flash factory

$(PROGRAM).ihx: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.rel : %.c $(HEADERS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

CCOMPILEDFILES=$(SOURCES:.c=.asm) $(SOURCES:.c=.lst) $(SOURCES:.c=.rel) \
               $(SOURCES:.c=.rst) $(SOURCES:.c=.sym)
clean:
	rm -f $(PROGRAM).ihx $(PROGRAM).cdb $(PROGRAM).lk $(PROGRAM).map $(CCOMPILEDFILES)

flash: $(PROGRAM).ihx
	$(FLASHER) -c $(PROGRAMMER) -p $(DEVICE) -w $(PROGRAM).ihx

factory: $(FACTORYOPT)
	$(FLASHER) -c $(PROGRAMMER) -p $(DEVICE) -s opt -w $(FACTORYOPT)
