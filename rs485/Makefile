## Set Mcu Type
DEVICE=stm8s103f3
CPU=STM8S103

## A directory for common include files
BSP = bsp

## Get program name from enclosing directory name
PROGRAM = objects/$(lastword $(subst /, ,$(CURDIR)))

SOURCES=$(wildcard src/*.c)
OBJECTS=$(SOURCES:.c=.rel)

HEADERS=$(wildcard src/inc/*.h $(BSP)/inc/*.h)

CC = /Developer/sdcc/bin/sdcc
PROGRAMMER = stlinkv2
FLASHER = /Developer/sdcc/bin/stm8flash
FACTORYOPT = /Developer/sdcc/stm8s103_opt_factory_defaults.bin
MKLIB = scripts/compile-s.sh
CPULIB = $(BSP)/lib/$(CPU).lib

CPPFLAGS = -I$(BSP)/inc -Isrc/inc
CFLAGS = --Werror --std-sdcc99 -mstm8 -D$(CPU)
LDFLAGS = -lstm8 -mstm8 --out-fmt-ihx

.PHONY: all clean flash factory

$(PROGRAM).ihx: $(OBJECTS) $(CPULIB)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(CPULIB):
	$(MKLIB) $(CPU)

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
