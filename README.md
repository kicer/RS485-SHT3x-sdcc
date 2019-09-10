# RS485-SHT3x-sdcc
RS485 T/RH sensor monitor.

1. modbus
2. i2c.sht3x
3. task.sys
4. uart.interrupt
5. rs232/rs485

### Hardware Layout
```
                     +--------------+
      +------ PD5 ---| stm8s103/003 |---- PC3 --+
      |   +-- PD6 ---|  (n76e003)   |---- PC4   |
      |   |          +--------------+      |    |
    +-+---+-----+                          |    |
    | TTL-RS485 |    +--------------+      |    |
    | convertor |    |  SHT30DIS-B  |---- SDA   |
    +--+----+---+    |   (DHT12)    |---- SCL --+
       |    |        +--------------+
       A    B
```

### Test Suit

1. Read *info-reg*, length=6, addr0=0x0000
```
 PC>  01 03 00 00 00 07 04 08
MCU>  01 03 0E 00 00 00 01 25 80 00 05 00 01 00 1A 01 00 C2 D3 
        0E: data length
     00 00: reg address
     00 01: bus address
     25 80: com baudrate
     00 05: meas ticks(sec)
     00 01: report
     00 1A: power count
     01 00: sensor reg address
```

2. Read *sensor-reg*, length=4, addr0=0x0100
```
 PC>  01 03 01 00 00 04 45 F5
MCU>  01 03 08 01 00 04 EF 02 9A 00 02 1E 20 
        08: data length
     01 00: reg address
     04 EF: T = t/10-100
     02 9A: RH = rh/10
     00 02: measSucc count
 ```

3. Set *MeasSec* to 3
```
 PC>  01 06 00 04 00 01 39 CB
MCU>  01 06 00 03 00 03 39 CB
 ```

4. Enable *Auto-Report*
```
 PC>  01 06 00 04 00 01 09 CB
MCU>  01 06 00 04 00 01 09 CB
...
ACK>  01 00 00 04 01 04 04 F0 02 96 00 05
 ```
