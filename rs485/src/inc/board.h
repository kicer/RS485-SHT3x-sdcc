#ifndef _BOARD_H_
#define _BOARD_H_


#define UART_RX_MAXSIZE    64

#define EVENT_RECV_PKG     1
#define EVENT_SEND_PKG     2

#define MAGIC_CODE        19890818

typedef struct {
    uint8_t  com_address;
    uint32_t com_baud;
    uint32_t magic;
} DevState;

extern int board_init(void);


#endif /* _BOARD_H_ */
