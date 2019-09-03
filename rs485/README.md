# sdcc-template-STM8S_StdPeriph_Driver
sdcc template which use STM8S_StdPeriph_Driver(v2.3.1) in macOSx.

Thanks for [STM8-SPL_SDCC_patch](https://github.com/gicking/STM8-SPL_SDCC_patch)

## Install

1. path of compiler config in Makefile
```
CC = /Developer/sdcc/bin/sdcc
PROGRAMMER = stlinkv2
FLASHER = /Developer/sdcc/bin/stm8flash
FACTORYOPT = /Developer/sdcc/stm8s103_opt_factory_defaults.bin
```

2. install [sdcc](http://sdcc.sourceforge.net/)
3. install [stm8flash](https://github.com/vdudouyt/stm8flash)
4. touch stm8s103_opt_factory_defaults.bin
```
echo "00 00 ff 00 ff 00 ff 00 ff 00 ff" | xxd -r -p > /Developer/sdcc/stm8s103_opt_factory_defaults.bin
```

## Usage
1. Set DEVICE in Makefile
```
DEVICE=stm8s103f3
DEFINES=STM8S103
```

2. Set APPS in Makefile
```
APPS = gpio uart1 clk
```

3. Build ihex: `make`
4. Program: `make flash`
5. Clean: `make clean`
6. Factory of Option-Bytes: `make factory`
