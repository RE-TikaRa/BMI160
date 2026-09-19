#include "stm32f10x.h"
#include "board.h"
#include "bsp_uart.h"
#include <stdio.h>

static uint8_t uart_data[USART1_RECEIVE_LENGTH];

int main(void)
{
	board_init();
	board_led_init();
	uart1_init(115200U);

	while (1)
	{
		board_led_write(Bit_SET);
		printf("LED ON!\r\n");
		delay_ms(500U);

		board_led_write(Bit_RESET);
		printf("LED OFF!\r\n");
		delay_ms(500U);

		uint8_t overflow = 0U;
		uint16_t length = uart1_read(uart_data, sizeof uart_data, &overflow);
		if (length != 0U || overflow != 0U)
		{
			printf("data[%u] = %s\r\n", (unsigned)length, uart_data);
			if (overflow != 0U)
			{
				printf("data truncated\r\n");
			}
		}
	}
}
