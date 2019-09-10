#include "i2c_sw_master.h"

void I2c_Init(void) {
    /* Note: low-level init in board.init() */
    SDA_HIGH();
    SCL_HIGH();
    SDA_OUT();
    SCL_OUT();
}

void I2c_StartCondition(void) {
  SDA_HIGH();
  SCL_HIGH();
  I2C_NOP();
  SDA_LOW();
  I2C_LONG_NOP();  // hold time start condition (t_HD;STA)
  SCL_LOW();
  I2C_NOP();
}

void I2c_StopCondition(void) {
  SCL_LOW();
  SDA_LOW();
  I2C_NOP();
  SCL_HIGH();
  I2C_LONG_NOP();  // set-up time stop condition (t_SU;STO)
  SDA_HIGH();
  I2C_NOP();
}

int I2c_WriteByte(uint8_t txByte) {
    int ackBit = 0;
    uint8_t mask;
    for(mask = 0x80; mask > 0; mask >>= 1) {
        I2C_NOP();             // data hold time(t_HD;DAT)
        // shift bit for masking (8 times)
        if((mask & txByte) == 0) {
            SDA_LOW();
        } else {
            SDA_HIGH();
        }
        I2C_NOP();             // data set-up time (t_SU;DAT)
        SCL_HIGH();                       // generate clock pulse on SCL
        I2C_LONG_NOP();            // SCL high time (t_HIGH)
        SCL_LOW();
    }
    I2C_LONG_NOP();
    SDA_IN();                             // release SDA-line
    SCL_HIGH();
    I2C_LONG_NOP();                 // data set-up time (t_SU;DAT)
    if(SDA_READ) ackBit = 1;              // check ack from i2c slave
    SCL_LOW();
    I2C_NOP();                // wait to see byte package on scope
    SDA_OUT();
    return ackBit;                        // return error code
}

static int I2c_WaitWhileClockStreching(int timeout) {
    int busy = 0;
    if(timeout > 0) {
        SCL_IN();
        while(timeout-- > 0) {
            I2C_NOP();
            if(SCL_READ) break;
            if(timeout == 0) busy = 1;
        }
        SCL_OUT();
        SCL_HIGH();
    }
    return busy;
}

int I2c_ReadByte(uint8_t *rxByte, uint8_t ackBit) {
    int error = 0;
    uint8_t mask;
    *rxByte = 0x00;
    SDA_IN();                              // release SDA-line
    for(mask = 0x80; mask > 0; mask >>= 1) {
        I2C_NOP();                // data hold time(t_HD;DAT)
        // shift bit for masking (8 times)
        SCL_HIGH();               // start clock on SCL-line
        I2C_NOP();                // clock set-up time (t_SU;CLK)
        error += I2c_WaitWhileClockStreching(I2C_CS_TICKS);// wait while clock streching
        if(SDA_READ) *rxByte |= mask;        // read bit
        SCL_LOW();
    }
    I2C_LONG_NOP();                // data hold time(t_HD;DAT)
    if(ackBit) { /* NACK */
        SDA_OUT();
        SDA_HIGH();
    } else { /* ACK */
        SDA_OUT();
        SDA_LOW();
    }
    I2C_NOP();                  // data set-up time (t_SU;DAT)
    SCL_HIGH();                            // clk #9 for ack
    I2C_LONG_NOP();                 // SCL high time (t_HIGH)
    SCL_LOW();
    SDA_HIGH();                            // release SDA-line
    I2C_LONG_NOP();                 // wait to see byte package on scope
    return error;                          // return with no error
}
