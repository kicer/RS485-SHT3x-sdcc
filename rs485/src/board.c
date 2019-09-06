#include "stm8s.h"
#include "sys.h"
#include "uart.h"
#include "eeprom.h"
#include "board.h"

uint8_t Rx1Buffer[UART_RX_MAXSIZE];
DevState gDevSt;

static void config_read_state(DevState *pst) {
    int size = eeprom_read_config(pst);
    if((size != sizeof(DevState))||(pst->magic != MAGIC_CODE)) {
        pst->magic = MAGIC_CODE;
        pst->com_address = 1;
        pst->com_baud = 9600;
        eeprom_write_config(&gDevSt, sizeof(DevState));
    }
}

static void uart_recv_pkg_cb(void) {
}

static void user_key_cb(void) {
    static uint32_t key_pressed = 0;
    if(GPIO_ReadInputPin(GPIOC, GPIO_PIN_5)==RESET) {
        key_pressed += 1;
        if(key_pressed > 200) { /* >2s */
            //LED_ON();
        }
    } else {
        if(key_pressed > 200) { /* >2.0s */
            //LED_OFF();
        }
        key_pressed = 0;
    }
}

int board_init(void) {
    /* read config */
    eeprom_init();
    config_read_state(&gDevSt);
    /* init com */
    uart1_init(gDevSt.com_baud);
    /* register event */
    sys_task_reg_timer(10, user_key_cb);
    sys_task_reg_event(EVENT_RECV_PKG, uart_recv_pkg_cb);
    //sys_task_reg_event(EVENT_SEND_PKG, action_led_off);
    sys_task_reg_alarm(60000, config_update_powerCnt);    /* 1min */
    return 0;
}

void gpioAExti_cb(uint8_t val) {
    try_param(val);
}

void gpioBExti_cb(uint8_t val) {
    try_param(val);
}

void gpioCExti_cb(uint8_t val) {
    try_param(val);
}

void gpioDExti_cb(uint8_t val) {
    try_param(val);
}

void uart1_rx_cb(uint8_t ch) {
    if(ch == 0xAA) { /* receive a valid package? */
        sys_event_trigger(EVENT_RECV_PKG);
    }
}
