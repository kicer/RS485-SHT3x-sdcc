#include "stm8s.h"
#include "sys.h"
#include "uart.h"
#include "eeprom.h"
#include "board.h"
#include "modbus.h"


/* device info regMap
 * +------+------+------+---------+--------+----------+------+
 * |  0   |  1   |  2   |    3    |   4    |    5     |   6  |
 * +------+------+------+---------+--------+----------+------+
 * | ILoc | Addr | Baud | MeasSec | Report | PowerCnt | RLoc |
 * +------+------+------+---------+--------+----------+------+
 */
uint16_t g_reg_info[modbus_REGSIZE(7)];
#define MODBUS_CFG_ILoc      modbus_reg(g_reg_info, 0)
#define MODBUS_CFG_Addr      modbus_reg(g_reg_info, 1)
#define MODBUS_CFG_Baud      modbus_reg(g_reg_info, 2)
#define MODBUS_CFG_MeasSec   modbus_reg(g_reg_info, 3)
#define MODBUS_CFG_Report    modbus_reg(g_reg_info, 4)
#define MODBUS_CFG_PowerCnt  modbus_reg(g_reg_info, 5)
#define MODBUS_CFG_RLoc      modbus_reg(g_reg_info, 6)

/* sensor data regMap
 * +------+------+------+---------+
 * | 0100 | 0101 | 0102 |   0103  |
 * +------+------+------+---------+
 * | RLoc | Temp | Humi | SuccCnt |
 * +------+------+------+---------+
 */
uint16_t g_reg_sensor[modbus_REGSIZE(4)];
#define MODBUS_REG_RLoc      modbus_reg(g_reg_sensor, 0)
#define MODBUS_REG_Temp      modbus_reg(g_reg_sensor, 1)
#define MODBUS_REG_Humi      modbus_reg(g_reg_sensor, 2)
#define MODBUS_REG_SuccCnt   modbus_reg(g_reg_sensor, 3)

__IO uint8_t Rx1Buffer[32];
__IO DevState gDevSt;


static uint16_t crc16_update(uint16_t crc, uint8_t a) {
    uint8_t i;
    crc ^= (uint16_t)a;
    for (i=0; i<8; i++) {
        if(crc & 1) {
            crc = (crc >> 1) ^ 0xA001;
        } else {
            crc = (crc >> 1);
        }
    }
    return crc;
}

static void config_read_state(DevState *pst) {
    int size = eeprom_read_config(pst);
    if((size != sizeof(DevState))||(pst->magic != MAGIC_CODE)) {
        pst->magic = MAGIC_CODE;
        pst->comAddress = 1;
        pst->comBaud = 9600;
        pst->measSeconds = 5;
        pst->autoReport = 1;
        pst->powerCnt = 0;
    } else {
        if(pst->comBaud == 0) {
            pst->comBaud = 115200;
        }
    }
}

#include "i2c_sw_master.h"
#define I2C_WRITE   ((uint8_t)0)
#define I2C_READ    ((uint8_t)1)
#define SHT3x_ADDR  ((uint8_t)(0x44<<1))
static void sensor_read_cb(void) {
    int ack_err = 0;
    uint8_t rxByte[6];
    I2c_StartCondition();
    ack_err += I2c_WriteByte(SHT3x_ADDR+I2C_READ);
    if(ack_err == 0) {
        ack_err += I2c_ReadByte(rxByte+0, 0);
        ack_err += I2c_ReadByte(rxByte+1, 0);
        ack_err += I2c_ReadByte(rxByte+2, 0);
        ack_err += I2c_ReadByte(rxByte+3, 0);
        ack_err += I2c_ReadByte(rxByte+4, 0);
        ack_err += I2c_ReadByte(rxByte+5, 1);
        I2c_StopCondition();
    }
    if(ack_err == 0) {
        /* T = t/10-100
         * RH = rh/10 */
        uint16_t t = (uint32_t)1750*(((uint16_t)rxByte[0]<<8)+rxByte[1])/65535+550;
        uint16_t rh = (uint32_t)1000*(((uint16_t)rxByte[3]<<8)+rxByte[4])/65535;
        MODBUS_REG_Temp = t;
        MODBUS_REG_Humi = rh;
        MODBUS_REG_SuccCnt = 1+MODBUS_REG_SuccCnt;
        if(gDevSt.autoReport) { /* todo:dataFormat? */
            uart1_flush_output();
            uart1_send(g_reg_sensor, sizeof(g_reg_sensor));
        }
    }
}

static void sensor_read(void) {
    int ack_err = 0;
    I2c_StartCondition();
    ack_err += I2c_WriteByte(SHT3x_ADDR+I2C_WRITE);
    ack_err += I2c_WriteByte(0x24);
    ack_err += I2c_WriteByte(0x00);
    I2c_StopCondition();
    if(ack_err == 0) {
        sys_task_reg_alarm(15, sensor_read_cb);
    }
}

static void config_update_powerCnt(void) {
    gDevSt.powerCnt += 1;
    if(eeprom_write_config(&gDevSt, sizeof(DevState)) == 0) {
        MODBUS_CFG_PowerCnt = gDevSt.powerCnt;
    }
}

static void config_sync_all(void) {
    int sync = 0;
    if(gDevSt.comAddress != MODBUS_CFG_Addr) {
        gDevSt.comAddress = MODBUS_CFG_Addr;
        sync += 1;
    }
    if(gDevSt.comBaud != MODBUS_CFG_Baud) {
        gDevSt.comBaud = MODBUS_CFG_Baud;
        sync += 1;
    }
    if(gDevSt.autoReport != MODBUS_CFG_Report) {
        gDevSt.autoReport = MODBUS_CFG_Report;
        sync += 1;
    }
    if(gDevSt.measSeconds != MODBUS_CFG_MeasSec) {
        gDevSt.measSeconds = MODBUS_CFG_MeasSec;
        sync += 1;
    }
    if(sync > 0) {
        if(eeprom_write_config(&gDevSt, sizeof(DevState)) == 0) {
            WWDG_SWReset();
        }
    }
}

static void uart1_recv_pkg_cb(void) {
    int i;
    uint8_t pkgSend[32];
    uint8_t cmd = Rx1Buffer[1];
    uint16_t addr0 = ((uint16_t)Rx1Buffer[2]<<8)+Rx1Buffer[3];
    uint16_t size = ((uint16_t)Rx1Buffer[4]<<8)+Rx1Buffer[5];
    uint16_t *preg = 0;
    uint16_t crc, value, offset;
    if(cmd == 0x06) size = 1;
    if(modbus_reg_check(g_reg_info, addr0, size)) {
        preg = g_reg_info;
    } else if(modbus_reg_check(g_reg_sensor, addr0, size)) {
        preg = g_reg_sensor;
    } else return; /* invalid address */
    if(cmd == 0x03) {
        pkgSend[0] = gDevSt.comAddress;
        pkgSend[1] = cmd;
        pkgSend[2] = (size*2)&0xFF;
        for(i=0; i<size; i++) {
            offset = addr0+i-modbus_reg_addr0(preg);
            value = modbus_reg(preg, offset);
            pkgSend[3+i*2] = (value>>8)&0xFF;
            pkgSend[3+i*2+1] = value&0xFF;
        }
        crc = 0xFFFF;
        for(i=0; i<size*2+3; i++) {
            crc = crc16_update(crc, pkgSend[i]);
        }
    } else if(preg == g_reg_info) {
        /* info-reg support write */
        offset = addr0-modbus_reg_addr0(preg);
        if(cmd == 0x06) { /* single word */
            value = ((uint16_t)Rx1Buffer[4]<<8)+Rx1Buffer[5];
            modbus_reg(preg, offset) = value;
        } else if(cmd == 0x10) { /* multi words */
            for(i=0; i<size; i++) {
                value = ((uint16_t)Rx1Buffer[7+2*i]<<8)+Rx1Buffer[8+2*i];
                modbus_reg(preg, offset+i) = value;
            }
        } else return; /* invalid cmd */
        crc = 0xFFFF;
        for(i=0; i<6; i++) {
            pkgSend[i] = Rx1Buffer[i];
            crc = crc16_update(crc, pkgSend[i]);
        }
        sys_task_reg_alarm(100, config_sync_all); /* 100ms */
    } else return; /* invalid pkg */
    pkgSend[i] = (crc>>8)&0xFF;
    pkgSend[i+1] = crc&0xFF;
    uart1_flush_output(); /* force output */
    uart1_send(pkgSend, i+2);
}

static void modbus_regs_init(void) {
    modbus_reg_init(g_reg_info, 0x0000, 7);
    modbus_reg_init(g_reg_sensor, 0x0100, 4);
    /* init info's reg */
    MODBUS_CFG_ILoc = 0x0000;
    MODBUS_CFG_Addr = gDevSt.comAddress;
    MODBUS_CFG_Baud = gDevSt.comBaud;
    MODBUS_CFG_MeasSec = gDevSt.measSeconds;
    MODBUS_CFG_Report = gDevSt.autoReport;
    MODBUS_CFG_PowerCnt = gDevSt.powerCnt;
    MODBUS_CFG_RLoc = 0x0100; /* regx address */
    /* init sensor's reg */
    MODBUS_REG_RLoc = 0x0100;
    MODBUS_REG_Temp = 0;
    MODBUS_REG_Humi = 0;
    MODBUS_REG_SuccCnt = 0;
}

int board_init(void) {
    /* read config */
    eeprom_init();
    config_read_state(&gDevSt);
    /* init com */
    uart1_init(gDevSt.comBaud);
    /* modbus init */
    modbus_regs_init();
    /* sensor init */
    I2c_Init();
    /* register event */
    sys_task_reg_timer(gDevSt.measSeconds*1000, sensor_read); /* read sens */
    sys_task_reg_event(EVENT_UART1_RECV_PKG, uart1_recv_pkg_cb); /* recv_pkg event callback */
    sys_task_reg_alarm(60000, config_update_powerCnt); /* update powerCnt after power.1min */
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
    /* modbus: <addr> 03 <reg_addr_2> <reg_size_2> <chksum_2> */
    static uint8_t idx = 0;
    static uint8_t chksum = 0;
    static uint16_t len = 0;
    static uint16_t crc = 0xFFFF;
    Rx1Buffer[idx] = ch;
    switch(idx) {
        case 0:
            if(ch == gDevSt.comAddress) idx+=1;
            crc = crc16_update(0xFFFF, ch);
            break;
        case 1:
            crc = crc16_update(crc, ch);
            idx+=1;
            if((ch==0x03)||(ch==0x06)) {
                len = 8;
            } else if(ch == 0x10) {
                len = 9; /* +n */
            } else {
                idx = 0;
            }
            break;
        default:
            if((len>8) && (idx==6)) {
                len += ch; /* cmd=0x10, fix length */
            }
            if(idx == len-2) {
                if((crc&0xFF) == ch) {
                    idx += 1;
                } else {
                    idx = 0;
                }
            } else if(idx == len-1) {
                idx = 0;
                if(((crc>>8)&0xFF) == ch) {
                    sys_event_trigger(EVENT_UART1_RECV_PKG);
                }
            } else {
                crc = crc16_update(crc, ch);
                idx += 1;
            }
            break;
    }
}
