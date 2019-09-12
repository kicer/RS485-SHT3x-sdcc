#ifndef _BOARD_H_
#define _BOARD_H_


#define EVENT_UART1_RECV_PKG     1
#define EVENT_UART1_SEND_PKG     2

#define MAGIC_CODE        0x19890818

typedef struct {
    uint8_t  comAddress;
    uint32_t comBaud;
    uint8_t  dataFormat;
    uint8_t  autoReport;
    uint16_t measSeconds;
    uint16_t powerCnt;
    uint32_t magic;
} DevState;

extern int board_init(void);


#endif /* _BOARD_H_ */
