#include "stm8s.h"
#include <stdint.h>

#define assert_param(x) ((void)x)

int hw_init(void) {
    /* HSIDIV[4:3]; CPUDIV[2:0]
     * Master Clock=4MHz; CPU Clock=512KHz
     * HSI RC 16MHz --/4--> 4MHz --/8--> 512KHz
     * Refer: AN2857 Application note
     */
    CLK_CKDIVR = 0x13;
    /* close all peripheral clock */
    CLK_PCKENR1 = 0x00;
    CLK_PCKENR2 = 0x00;

    /* GPIO */
    PA_DDR = 0xFF;
    PB_DDR = 0xFF;
    PC_DDR = 0xFF;
    PD_DDR = 0xFF;
    PA_ODR = 0x00;
    PB_ODR = 0x00;
    PC_ODR = 0x00;
    PD_ODR = 0x00;

    enableInterrupts();

    return 0;
}

int hw_sleep(int sec) {
    const uint8_t APR_Array[] = {62, 23, 23, 61};
    const uint8_t TBR_Array[] = {15, 15, 14, 12};
    const uint8_t ticks[] = {30,12,2,1};
    CLK_PCKENR2 |= CLK_AWU;
    while(sec > 0) {
        for(int i=0; i<sizeof(ticks); i++) {
            uint8_t _delay = ticks[i];
            if(sec >= _delay) {
                AWU_CSR = AWU_CSR_AWUEN;
                AWU_TBR = TBR_Array[i];
                AWU_APR = APR_Array[i];
                CLK_ICKR |= CLK_ICKR_REGAH;
                FLASH_CR1 |= FLASH_CR1_AHALT;
                halt();
                CLK_ICKR &= ~CLK_ICKR_REGAH;
                FLASH_CR1 &= ~FLASH_CR1_AHALT;
                sec -= _delay;
                break;
            }
        }
    }
    CLK_PCKENR2 &= ~CLK_AWU;
    return 0;
}

int read_user_key(void) {
    return 1;
}

int read_fast_mode(void) {
    return 0;
}

int flash_read_config(uint8_t *data, int size) {
    assert_param(data);
    assert_param(size);
    return -1;
}

int uart_init(void) {
    return 0;
}

int lcd_init(int lcd_type) {
    assert_param(lcd_type);
    return 0;
}

int lcd_show(uint8_t *data, int size) {
    assert_param(data);
    assert_param(size);
    return -1;
}

int rf_init(int rf_type) {
    assert_param(rf_type);
    return 0;
}

int rf_send(uint8_t *data, int size) {
    assert_param(data);
    assert_param(size);
    return -1;
}

int sensor_init(int sensor_type) {
    assert_param(sensor_type);
    return 0;
}

int sensor_read(int sensor_type, uint8_t *data, int size) {
    assert_param(sensor_type);
    assert_param(data);
    assert_param(size);
    return 0;
}

volatile uint8_t g_address = 0;
volatile uint8_t g_lcd = 0;
volatile uint8_t g_rf = 1;
volatile uint8_t g_sensor = 0;
volatile uint8_t g_fast = 30;
volatile uint8_t g_xslow = 2;

void main(void) {
    uint8_t ticks = 60;

    /* Ax L? R? S? Fx Xn*/
    uint8_t bytes_cfg[6];


    /* 硬件相关初始化 */
    hw_init();

    /* 读取user按键 */
    if(read_user_key()) {
        /* 初始化串口 */
        uart_init();
    }

    if(flash_read_config(bytes_cfg, sizeof(bytes_cfg)) == 0) {
        g_address = bytes_cfg[0];
        g_lcd = bytes_cfg[1];
        g_rf = bytes_cfg[2];
        g_sensor = bytes_cfg[3];
        g_fast = bytes_cfg[4];
        g_xslow = bytes_cfg[5];
    }

    if(read_fast_mode()) { /* 快速模式 */
        ticks = g_fast;
    } else { /* 慢速模式 */
        ticks = g_fast*g_xslow;
    }

    lcd_init(g_lcd);
    sensor_init(g_sensor);
    rf_init(g_rf);

    ticks = 5;
    PORT(PB, DDR) |= PIN5;
    PORT(PB, CR1) |= PIN5;
    while(1) {
        uint8_t data[4];
        if(sensor_read(g_sensor, data, sizeof(data)) == 0) {
            if(g_rf) rf_send(data, sizeof(data));
            if(g_lcd) lcd_show(data, sizeof(data));
        }
        hw_sleep(ticks);
        PORT(PB, ODR) ^= PIN5;
    }
}

void awu_isr(void) __interrupt(AWU_IRQ) {
    AWU_CSR = AWU_CSR;
}
