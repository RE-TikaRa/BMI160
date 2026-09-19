#ifndef BSP_UART_H
#define BSP_UART_H

#include "stm32f10x.h"

#define USART1_RECEIVE_LENGTH 1024U

extern uint8_t u1_recv_buff[USART1_RECEIVE_LENGTH];
extern volatile uint16_t u1_recv_length;
extern volatile uint16_t u1_recv_frame_length;
extern volatile uint8_t u1_recv_flag;
extern volatile uint8_t u1_recv_overflow;

void uart1_init(uint32_t baud);
void uart1_irq_handler(void);

void uart1_receive_clear(void);
uint8_t *uart1_get_data(void);
uint16_t uart1_get_data_length(void);
uint8_t uart1_get_overflow(void);
uint16_t uart1_read(uint8_t *data, uint16_t capacity, uint8_t *overflow);

#endif
