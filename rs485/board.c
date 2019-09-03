#include "stm8s.h"
#include "sys.h"
#include "uart.h"
#include "eeprom.h"
#include "board.h"

uint8_t Rx1Buffer[UART_RX_MAXSIZE];
DevState gDevSt;
__IO uint8_t actionState = 0;

#define LED_ON()   GPIO_WriteHigh(GPIOA, GPIO_PIN_3)
#define LED_OFF()  GPIO_WriteLow(GPIOA, GPIO_PIN_3)


static uint8_t config_make_checksum(void *pkg, int size) {
    uint8_t chksum = 0;
    uint8_t *pst = (uint8_t *)pkg;
    pst[size-2] = 0;
    for(int i=0; i<size; i++) {
        chksum += *(pst+i);
    }
    return (0xFF-chksum);
}

static void config_read_state(DevState *pst) {
    int size = eeprom_read_config(pst);
    uint8_t chksum = pst->chksum;
    if(
        (size != sizeof(DevState)) ||
        (pst->len != 8) ||
        (chksum!=config_make_checksum(pst, sizeof(DevState)))
    ){
        pst->head = 0x55;
        pst->cmd = 0x01;
        pst->len = 8;
        pst->powerCnt = 0;
        pst->actionCnt = 0;
        pst->devState = 0;
        pst->tuiganRunTime = TUIGAN_RUN_TIME;
        pst->actionLockCount = ACTION_LOCK_COUNT;
        pst->bmqRunCircle = BMQ_RUN_CIRCLE;
        pst->bmqRunAngle = BMQ_RUN_ANGLE;
        pst->tail = 0xAA;
        pst->chksum = config_make_checksum(&pst, sizeof(DevState));
    }
}

static void config_update_config(void) {
    gDevSt.devState = actionState;
    gDevSt.chksum = config_make_checksum(&gDevSt, sizeof(DevState));
    eeprom_write_config(&gDevSt, sizeof(DevState));
}

static void config_update_powerCnt(void) {
    gDevSt.powerCnt += 1;
    config_update_config();
}

static void config_update_actionCnt(void) {
    gDevSt.actionCnt += 1;
    config_update_config();
}

static void action_led_off(void) {
    LED_OFF();
}

__IO uint32_t bmqCh1,bmqCh2,bmqCh3,bmqCh4;
__IO uint32_t bmqMaxCount=0;

static void bmq_data_clear(void) {
    bmqCh1 = bmqCh2 = bmqCh3 = bmqCh4 = 0;
}

static void action_step_finish(void) {
    actionState = 6;
    /* stop tuigan */
    GPIO_WriteLow(GPIOC, GPIO_PIN_4);
    GPIO_WriteLow(GPIOC, GPIO_PIN_3);
    /* update state */
    config_update_actionCnt();
    /* reinit after xxs */
    actionState = 0;
}

static void action_step_down(void) {
    actionState = 5;
    /* down tuigan */
    GPIO_WriteLow(GPIOC, GPIO_PIN_4);
    GPIO_WriteHigh(GPIOC, GPIO_PIN_3);
    sys_task_reg_alarm((clock_t)gDevSt.tuiganRunTime*1000, action_step_finish);
}

static void action_step_stop(void) {
    actionState = 4;
    /* stop motor */
    GPIO_WriteLow(GPIOB, GPIO_PIN_4);
    GPIO_WriteLow(GPIOD, GPIO_PIN_3);
    sys_task_reg_alarm((clock_t)2000, action_step_down);
}

static void action_step_move(void) {
    actionState = 3;
    /* stop tuigan */
    GPIO_WriteLow(GPIOC, GPIO_PIN_4);
    GPIO_WriteLow(GPIOC, GPIO_PIN_3);
    /* bmq round-length */
    bmq_data_clear();
    bmqMaxCount = gDevSt.bmqRunCircle*100+gDevSt.bmqRunAngle;
    /* start motor */
    GPIO_WriteLow(GPIOB, GPIO_PIN_4);
    GPIO_WriteHigh(GPIOD, GPIO_PIN_3);
}

static void action_step_up(void) {
    actionState = 2;
    /* up tuigan */
    GPIO_WriteLow(GPIOC, GPIO_PIN_3);
    GPIO_WriteHigh(GPIOC, GPIO_PIN_4);
    sys_task_reg_alarm((clock_t)gDevSt.tuiganRunTime*1000, action_step_move);
}

static void action_step_start(void) {
    actionState = 1;
    sys_task_reg_alarm((clock_t)1500, action_step_up);
}

static void uart_recv_pkg_cb(void) {
    int cmd = Rx1Buffer[1];
    int size = Rx1Buffer[2];
    if(cmd == 0x01) { /* readState */
        if(size == 5) {
            gDevSt.tuiganRunTime = Rx1Buffer[3];
            gDevSt.actionLockCount = Rx1Buffer[4];
            gDevSt.bmqRunCircle = Rx1Buffer[5];
            gDevSt.bmqRunAngle = Rx1Buffer[6];
            sys_task_reg_alarm(1000, config_update_config);
        }
        if(gDevSt.devState != actionState) {
            gDevSt.devState = actionState;
            gDevSt.chksum = config_make_checksum(&gDevSt, sizeof(DevState));
        }
        uart1_flush_output(); /* force output */
        uart1_send((uint8_t *)&gDevSt, sizeof(gDevSt));
        LED_ON();
    } else if(cmd == 0x02) { /* filterUpdate */
        if(actionState == 0) {
            if(gDevSt.actionCnt < gDevSt.actionLockCount) {
                /* up > move > check > stop > down */
                action_step_start();
            }
        }
        /* ack state */
        ActionState st= {0x55,0x02,2,0,0,0xAA};
        st.step = actionState;
        st.chksum = config_make_checksum(&st, sizeof(ActionState));
        uart1_flush_output(); /* force output */
        uart1_send((uint8_t *)&st, sizeof(st));
        LED_ON();
    }
}

static void user_key_cb(void) {
    static int tuigan_action = 0;
    static uint32_t key_pressed = 0;
    if(GPIO_ReadInputPin(GPIOC, GPIO_PIN_5)==RESET) {
        key_pressed += 1;
        if(key_pressed > 200) { /* >2s */
            LED_ON();
            if(tuigan_action) {
                /* up tuigan */
                GPIO_WriteLow(GPIOC, GPIO_PIN_3);
                GPIO_WriteHigh(GPIOC, GPIO_PIN_4);
            } else {
                /* down tuigan */
                GPIO_WriteLow(GPIOC, GPIO_PIN_4);
                GPIO_WriteHigh(GPIOC, GPIO_PIN_3);
            }
        }
    } else {
        if(key_pressed > 200) { /* >2.0s */
            LED_OFF();
            if(gDevSt.actionCnt != 0) {
                gDevSt.actionCnt = 0;
                config_update_config();
            }
            /* stop tuigan */
            GPIO_WriteLow(GPIOC, GPIO_PIN_4);
            GPIO_WriteLow(GPIOC, GPIO_PIN_3);
            tuigan_action = !tuigan_action;
        }
        key_pressed = 0;
    }
}

int board_init(void) {
    uart1_init(UART_COMM_BAUD);
    /* indicate led */
    GPIO_Init(GPIOA, GPIO_PIN_3, GPIO_MODE_OUT_PP_LOW_FAST);
    /* reset key */
    GPIO_Init(GPIOC, GPIO_PIN_5, GPIO_MODE_IN_PU_NO_IT);
    /* 4ch motor output */
    GPIO_Init(GPIOD, GPIO_PIN_3, GPIO_MODE_OUT_PP_LOW_FAST);
    GPIO_Init(GPIOB, GPIO_PIN_4, GPIO_MODE_OUT_OD_LOW_FAST);
    GPIO_Init(GPIOC, GPIO_PIN_3, GPIO_MODE_OUT_PP_LOW_FAST);
    GPIO_Init(GPIOC, GPIO_PIN_4, GPIO_MODE_OUT_PP_LOW_FAST);
    /* 3ch bmq input */
    GPIO_Init(GPIOC, GPIO_PIN_6, GPIO_MODE_IN_PU_IT);
    GPIO_Init(GPIOC, GPIO_PIN_7, GPIO_MODE_IN_PU_IT);
    GPIO_Init(GPIOD, GPIO_PIN_2, GPIO_MODE_IN_PU_IT);
    EXTI_SetExtIntSensitivity(EXTI_PORT_GPIOD, EXTI_SENSITIVITY_FALL_ONLY);
    EXTI_SetExtIntSensitivity(EXTI_PORT_GPIOC, EXTI_SENSITIVITY_FALL_ONLY);
    /* read config */
    eeprom_init();
    config_read_state(&gDevSt);
    /* register event */
    sys_task_reg_timer(10, user_key_cb);
    sys_task_reg_event(EVENT_RECV_PKG, uart_recv_pkg_cb);
    sys_task_reg_event(EVENT_SEND_PKG, action_led_off);
    sys_task_reg_alarm(60000, config_update_powerCnt);    /* 1min */
    sys_task_reg_event(EVENT_BMQ_STOP, action_step_stop);
    for(int i=0; i<10; i++) {
      LED_ON();
      for(int j=0; j<100; j++) for(int k=0; k<10000; k++) {nop();}
      LED_OFF();
      for(int j=0; j<100; j++) for(int k=0; k<10000; k++) {nop();}
    };
    return 0;
}


void gpioCExti_cb(uint8_t val) {
    uint32_t cnt = bmqMaxCount;
    if((val&GPIO_PIN_6)==0x00) {
        bmqCh1 += 1;
        if((cnt>0)&&(bmqCh1 >= cnt)) {
            bmqMaxCount = 0;
            sys_event_trigger(EVENT_BMQ_STOP);
        }
    }
    if((val&GPIO_PIN_7)==0x00) {
        bmqCh2 += 1;
        if((cnt>0)&&(bmqCh2 >= cnt)) {
            bmqMaxCount = 0;
            sys_event_trigger(EVENT_BMQ_STOP);
        }
    }
}

void gpioDExti_cb(uint8_t val) {
    uint32_t cnt = bmqMaxCount;
    if((val&GPIO_PIN_2)==0x00) {
        bmqCh3 += 1;
        if((cnt>0)&&(bmqCh3 >= cnt)) {
            bmqMaxCount = 0;
            sys_event_trigger(EVENT_BMQ_STOP);
        }
    }
}

void uart1_rx_cb(uint8_t ch) {
    /* package: 55 [cmd] [len] <data> [chksum] AA */
    static int idx = 0;
    static int size = 0;
    static uint8_t chksum = 0;
    Rx1Buffer[idx] = ch;
    switch(idx) {
        case 0:
            if(ch == 0x55) idx+=1;
            break;
        case 1:
            if(ch < 0x03) { /* cmd=1,2 */
                idx += 1;
                chksum = ch;
            } else {
                idx = 0;
            }
            break;
        case 2:
            if(ch <= UART_RX_MAXSIZE) {
                idx += 1;
                size = ch;
                chksum += ch;
                if(ch == 0) idx = 0;
            } else {
                idx = 0;
            }
            break;
        default:
            idx += 1;
            chksum += ch;
            break;
    }
    if(idx >= size+4) {
        if(ch == 0xAA) { /* receive a valid package? */
            sys_event_trigger(EVENT_RECV_PKG);
        }
        idx = 0;
    } else if(idx == size+3) {
        if(chksum != 0) idx = 0;
    }
}