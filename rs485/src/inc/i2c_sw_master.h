#ifndef _I2C_SW_MASTER_H_
#define _I2C_SW_MASTER_H_


#include <stdint.h>
#include "stm8s.h"

/* -- adapt the defines for your uC -- */
// SDA on port C, bit 4
#define SDA_IN()   do {GPIOC->DDR &= ~(1<<4); GPIOC->CR1 |= (1<<4);} while(0)
#define SDA_OUT()  do {GPIOC->DDR |= (1<<4);  GPIOC->CR1 |= (1<<4);} while(0)
#define SDA_LOW()  (GPIOC->ODR &= ~(1<<4))
#define SDA_HIGH() (GPIOC->ODR |= (1<<4) )
#define SDA_READ   ((GPIOC->IDR&(1<<4)) != 0)
// SCL on port C, bit 3
#define SCL_IN()   do {GPIOC->DDR &= ~(1<<3); GPIOC->CR1 |= (1<<3);} while(0)
#define SCL_OUT()  do {GPIOC->DDR |= (1<<3) ; GPIOC->CR1 |= (1<<3);} while(0)
#define SCL_LOW()  (GPIOC->ODR &= ~(1<<3))
#define SCL_HIGH() (GPIOC->ODR |= (1<<3) )
#define SCL_READ   ((GPIOC->IDR&(1<<3)) != 0)

#define I2C_NOP()  do {              \
    for(uint32_t i=0; i<10; i++) {  \
        nop();                       \
    }} while(0)

#define I2C_LONG_NOP()  do {         \
    for(uint32_t i=0; i<20; i++) { \
        nop();                       \
    }} while(0)

/* clock streching timeout(0 for disabled) */
#define I2C_CS_TICKS        0


/* Initializes the ports for I2C interface. */
void I2c_Init(void);

/* Writes a start condition on I2C-Bus. */
//       _____
// SDA:       |_____
//       _______
// SCL:         |___
void I2c_StartCondition(void);

/* Writes a stop condition on I2C-Bus. */
//              _____
// SDA:   _____|
//            _______
// SCL:   ___|
void I2c_StopCondition(void);

/* @return: ackBit (0:ACK, 1:NACK) */
int I2c_WriteByte(uint8_t txByte);

/* @param: ackBit (0:ACK, 1:NACK) */
/* @return: error count */
int I2c_ReadByte(uint8_t *rxByte, uint8_t ackBit);


#endif /* _I2C_SW_MASTER_H_ */
