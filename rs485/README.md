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
 PC>  01 03 00 00 00 06 C5 C8
MCU>  01 03 0C 00 01 25 80 00 05 00 00 00 18 01 00 A7 33
        0C: data length
     00 01: bus address
     25 80: com baudrate
     00 05: meas ticks(sec)
     00 00: no report
     00 18: power count
     01 00: reg0 address
```

2. Read *sensor-reg*, length=4, addr0=0x0100
```
 PC>  01 03 01 00 00 04 45 F5
MCU>  01 03 08 01 04 04 EF 02 98 00 05 DC 85
        08: data length
     01 04: bus address, regx length
     04 EF: T = t/10-100
     02 98: RH = rh/10
     00 05: measSucc count
```

2. Set *Auto-Report*
```
 PC>  01 06 00 03 00 01 B8 0A
MCU>  01 06 00 03 00 01 B8 0A
...
ACK>  01 00 00 04 01 04 04 F0 02 96 00 05
 ```
