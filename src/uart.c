#include "stm8s.h"
#include "uart.h"

int uart1_init(unsigned long baud) {
    UART1_DeInit();
    /* Configure the UART1 */
    UART1_Init((uint32_t)baud, UART1_WORDLENGTH_8D, UART1_STOPBITS_1, UART1_PARITY_NO,
                UART1_SYNCMODE_CLOCK_DISABLE, UART1_MODE_TXRX_ENABLE);

    UART1_ITConfig(UART1_IT_RXNE_OR, ENABLE);
    UART1_Cmd(ENABLE);

    return 0;
}

uint8_t Tx1Buffer[UART_TX_MAXSIZE];
__IO int Tx1Counter = 0;
__IO int Tx1Pointer = 0;

#define UART1_TX_EMPTY()  (Tx1Counter==0)

int uart1_tx_data(void) {
    int ch = -1;
    int count = Tx1Counter;
    if(count > 0) {
        ch = Tx1Buffer[Tx1Pointer];
        Tx1Pointer += 1;
        if(Tx1Pointer >= count) {
            Tx1Counter = 0;
            Tx1Pointer = 0;
        }
    }
    return ch;
}

int uart1_flush_output(void) {
    Tx1Counter = 0;
    Tx1Pointer = 0;
    return 0;
}

int uart1_send(uint8_t *buffer, int size) {
    if(UART1_TX_EMPTY() && (size <= UART_TX_MAXSIZE) && (size > 0)) {
        for(int i=0; i<size; i++) {
            Tx1Buffer[i] = buffer[i];
        }
        Tx1Pointer = 0;
        Tx1Counter = size;
        //UART1_SendData8(uart1_tx_data());
        UART1_ITConfig(UART1_IT_TXE, ENABLE);
        return size;
    }
    return -1;
}
