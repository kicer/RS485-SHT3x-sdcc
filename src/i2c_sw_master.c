#include "i2c_sw_master.h"

void I2c_Init(void) {
    /* Note: low-level init in board.init() */
    SDA_HIGH(); SDA_OUT();
    SCL_HIGH(); SCL_OUT();
}

void I2c_StartCondition(void) {
    /* init to high */
    SDA_HIGH(); SCL_HIGH(); I2C_NOP();
    /* sda high->low when scl high */
    SDA_LOW(); I2C_LONG_NOP(); SCL_LOW();
    I2C_NOP();
}

void I2c_StopCondition(void) {
    /* init to low */
    SCL_LOW(); SDA_LOW(); I2C_NOP();
    /* sda low->high when scl high */
    SCL_HIGH(); I2C_LONG_NOP(); SDA_HIGH();
    I2C_NOP();
}

int I2c_WriteByte(uint8_t txByte) {
    int ackBit = 0;
    uint8_t mask;

    /* write byte */
    for(mask = 0x80; mask > 0; mask >>= 1) {
        I2C_NOP();
        if((mask & txByte) == 0) {
            SDA_LOW();
        } else {
            SDA_HIGH();
        }
        I2C_NOP();
        SCL_HIGH();
        I2C_LONG_NOP();
        SCL_LOW();
    }
    I2C_LONG_NOP();

    /* read ack */
    SDA_IN(); SCL_HIGH(); I2C_LONG_NOP();
    if(SDA_READ) ackBit = 1;

    /* to idle */
    SCL_LOW(); I2C_NOP(); SDA_OUT();
    return ackBit;
}

/* for low-speed device */
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

    /* read byte */
    SDA_IN(); 
    for(mask = 0x80; mask > 0; mask >>= 1) {
        I2C_NOP();
        SCL_HIGH();I2C_NOP();
        error += I2c_WaitWhileClockStreching(I2C_CS_TICKS);
        if(SDA_READ) *rxByte |= mask;
        SCL_LOW();
    }
    I2C_LONG_NOP();

    /* send ack */
    if(ackBit) { /* NACK */
        SDA_OUT();
        SDA_HIGH();
    } else { /* ACK */
        SDA_OUT();
        SDA_LOW();
    }
    I2C_NOP(); SCL_HIGH(); I2C_LONG_NOP();

    /* to idle */
    SCL_LOW(); SDA_HIGH(); I2C_LONG_NOP();
    return error;
}
