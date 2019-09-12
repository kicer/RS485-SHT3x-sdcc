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

__IO DevState gDevSt;

/* T = (-?)(t.d+t.p/10);  RH = rh.d+rh.p/10 */
#define SENSOR_DP_FORMAT  0
/* T = t/10-100;  RH = rh/10 */
#define SENSOR_XN_FORMAT  1


/* ################# helper functions #################   */
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

static uint8_t crc8_update(uint8_t crc, uint8_t a) {
    uint8_t bit;
    crc ^= (uint16_t)a;
    for (bit=8; bit>0; --bit){
        if(crc & 0x80) {
            crc = (crc << 1) ^ 0x0131;
        } else {
            crc = (crc << 1);
        }
    }
    return crc;
}


/* ################# modbus/config functions #################   */
static void config_read_state(DevState *pst) {
    int size = eeprom_read_config(pst);
    if((size != sizeof(DevState))||(pst->magic != MAGIC_CODE)) {
        pst->magic = MAGIC_CODE;
        pst->comAddress = 1;
        pst->comBaud = 9600;
        pst->measSeconds = 5;
        pst->dataFormat = 0;
        pst->autoReport = 1;
        pst->powerCnt = 0;
    } else {
        if(pst->comBaud == 0) {
            pst->comBaud = 115200;
        }
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
    uint16_t report = ((uint16_t)gDevSt.dataFormat<<8)+gDevSt.autoReport;
    if(report != MODBUS_CFG_Report) {
        gDevSt.dataFormat = (MODBUS_CFG_Report>>8)&0xFF;
        gDevSt.autoReport = MODBUS_CFG_Report&0xFF;
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

static void modbus_regs_init(void) {
    modbus_reg_init(g_reg_info, 0x0000, 7);
    modbus_reg_init(g_reg_sensor, 0x0100, 4);
    /* init info's reg */
    MODBUS_CFG_ILoc = 0x0000;
    MODBUS_CFG_Addr = gDevSt.comAddress;
    MODBUS_CFG_Baud = gDevSt.comBaud;
    MODBUS_CFG_MeasSec = gDevSt.measSeconds;
    MODBUS_CFG_Report = ((uint16_t)gDevSt.dataFormat<<8)+gDevSt.autoReport;
    MODBUS_CFG_PowerCnt = gDevSt.powerCnt;
    MODBUS_CFG_RLoc = 0x0100; /* regx address */
    /* init sensor's reg */
    MODBUS_REG_RLoc = 0x0100;
    MODBUS_REG_Temp = 0;
    MODBUS_REG_Humi = 0;
    MODBUS_REG_SuccCnt = 0;
}


/* ################# com functions #################   */
#define com_send(b,s) do {    \
/* switch to uart-send mode */\
        uart1_flush_output(); \
        uart1_send((b), (s)); \
    } while(0)

static void uart1_send_pkg_cb(void) {
    // switch to uart receive mode
}

__IO uint8_t Rx1Buffer[32]; /* uart1 receive buffer */

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
    pkgSend[i] = crc&0xFF;
    pkgSend[i+1] = (crc>>8)&0xFF;
    com_send(pkgSend, i+2);
}


/* ################# sensor functions #################   */
#include "i2c_sw_master.h"
#define I2C_WRITE   ((uint8_t)0)
#define I2C_READ    ((uint8_t)1)
#define SHT3x_ADDR  ((uint8_t)(0x44<<1))
#define DHT12_ADDR  ((uint8_t)(0xB8))

static void sensor_read_cb(void) {
    uint8_t ack_err = 0;
    uint8_t crc8,rxByte[6];
    uint16_t t = 0, rh = 0;
    I2c_StartCondition();
    ack_err += I2c_WriteByte(SHT3x_ADDR+I2C_READ);
    if(ack_err == 0) {
        crc8 = 0xFF;
        ack_err += I2c_ReadByte(rxByte+0, 0);
        crc8 = crc8_update(crc8, rxByte[0]);
        ack_err += I2c_ReadByte(rxByte+1, 0);
        crc8 = crc8_update(crc8, rxByte[1]);
        ack_err += I2c_ReadByte(rxByte+2, 0);
        if(crc8 != rxByte[2]) ack_err += 1;
        crc8 = 0xFF;
        ack_err += I2c_ReadByte(rxByte+3, 0);
        crc8 = crc8_update(crc8, rxByte[3]);
        ack_err += I2c_ReadByte(rxByte+4, 0);
        crc8 = crc8_update(crc8, rxByte[4]);
        ack_err += I2c_ReadByte(rxByte+5, 1);
        if(crc8 != rxByte[5]) ack_err += 1;
        I2c_StopCondition();
        t = (uint32_t)1750*(((uint16_t)rxByte[0]<<8)+rxByte[1])/65535+550;
        rh = (uint32_t)1000*(((uint16_t)rxByte[3]<<8)+rxByte[4])/65535;
        if(gDevSt.dataFormat == SENSOR_DP_FORMAT) {
            uint8_t t0 = t/10;
            uint8_t t1 = t%10;
            if(t0 > 100) {
                t0 -= 100;
            } else {
                t0 = 100 - t0;
                t1 = (10-t1)+0x80;
            }
            t = ((uint16_t)t0<<8)+t1;
            uint8_t rh0 = rh/10;
            uint8_t rh1 = rh%10;
            rh = ((uint16_t)rh0<<8)+rh1;
        }
    }
    if(ack_err != 0) { /* DHT12 */
        uint16_t chksum = 0;
        ack_err = 0;
        I2c_StartCondition();
        ack_err += I2c_WriteByte(DHT12_ADDR+I2C_WRITE);
        ack_err += I2c_WriteByte(0x00);
        I2c_StartCondition();
        ack_err += I2c_WriteByte(DHT12_ADDR+I2C_READ);
        for(int i=0; i<4; i++) {
            ack_err += I2c_ReadByte(rxByte+i, 0);
            chksum += rxByte[i];
        }
        ack_err += I2c_ReadByte(rxByte+4, 1);
        I2c_StopCondition();
        if(chksum == 0) ack_err += 1;
        if((chksum&0xFF) != rxByte[4]) {
            ack_err += 1;
        } else {
            if(gDevSt.dataFormat == SENSOR_DP_FORMAT) {
                rh = ((uint16_t)rxByte[0]<<8)+rxByte[1];
                t = ((uint16_t)rxByte[2]<<8)+rxByte[3];
            } else {
                rh = rxByte[1]+rxByte[0]*10;
                if(rxByte[3] >= 0x80) {
                    t = 1000-((rxByte[3]&0x7F)+rxByte[2]*10);
                } else {
                    t = 1000+rxByte[3]+rxByte[2]*10;
                }
            }
        }
    }
    if(ack_err == 0) {
        MODBUS_REG_Temp = t;
        MODBUS_REG_Humi = rh;
        MODBUS_REG_SuccCnt = 1+MODBUS_REG_SuccCnt;
        if(gDevSt.autoReport) {
            int i,size = 4; /* regSize = 4 */
            uint8_t pkgSend[32];
            pkgSend[0] = gDevSt.comAddress;
            pkgSend[1] = 0x03;
            pkgSend[2] = (size*2)&0xFF;
            for(i=0; i<size; i++) {
                uint16_t value = modbus_reg(g_reg_sensor, i);
                pkgSend[3+i*2] = (value>>8)&0xFF;
                pkgSend[3+i*2+1] = value&0xFF;
            }
            uint16_t crc = 0xFFFF;
            for(i=0; i<size*2+3; i++) {
                crc = crc16_update(crc, pkgSend[i]);
            }
            pkgSend[i] = crc&0xFF;
            pkgSend[i+1] = (crc>>8)&0xFF;
            com_send(pkgSend, i+2);
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
    try_param(ack_err);
    sys_task_reg_alarm(15, sensor_read_cb);
}


/* ################# main function #################   */
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
    sys_task_reg_event(EVENT_UART1_RECV_PKG, uart1_send_pkg_cb); /* recv_pkg event callback */
    sys_task_reg_alarm(60000, config_update_powerCnt); /* update powerCnt after power.1min */
    return 0;
}


/* ################# gpio-exti functions #################   */
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


/* ################# isr functions #################   */
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
