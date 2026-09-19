#include "bsp_uart.h"
#include <stdio.h>
#include <string.h>

uint8_t u1_recv_buff[USART1_RECEIVE_LENGTH];
volatile uint16_t u1_recv_length;
volatile uint16_t u1_recv_frame_length;
volatile uint8_t u1_recv_flag;
volatile uint8_t u1_recv_overflow;

void uart1_init(uint32_t baud)
{
	GPIO_InitTypeDef gpio;
	USART_InitTypeDef usart;
	NVIC_InitTypeDef nvic;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO |
	                       RCC_APB2Periph_GPIOA |
	                       RCC_APB2Periph_USART1,
	                       ENABLE);

	GPIO_StructInit(&gpio);
	gpio.GPIO_Pin = GPIO_Pin_9;
	gpio.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio);

	GPIO_StructInit(&gpio);
	gpio.GPIO_Pin = GPIO_Pin_10;
	gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &gpio);

	USART_DeInit(USART1);
	USART_StructInit(&usart);
	usart.USART_BaudRate = baud;
	usart.USART_WordLength = USART_WordLength_8b;
	usart.USART_StopBits = USART_StopBits_1;
	usart.USART_Parity = USART_Parity_No;
	usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_Init(USART1, &usart);

	uart1_receive_clear();
	USART_ClearFlag(USART1, USART_FLAG_RXNE | USART_FLAG_IDLE);
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);
	USART_Cmd(USART1, ENABLE);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	nvic.NVIC_IRQChannel = USART1_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 1U;
	nvic.NVIC_IRQChannelSubPriority = 1U;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);
}

#if !defined(__MICROLIB)
#if (__ARMCLIB_VERSION <= 6000000)
struct __FILE
{
	int handle;
};
#endif

FILE __stdout;

void _sys_exit(int status)
{
	(void)status;
}
#endif

int fputc(int ch, FILE *stream)
{
	(void)stream;
	USART_SendData(USART1, (uint8_t)ch);
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
	{
	}
	return ch;
}

void uart1_receive_clear(void)
{
	u1_recv_length = 0U;
	u1_recv_frame_length = 0U;
	u1_recv_flag = 0U;
	u1_recv_overflow = 0U;
}

uint8_t *uart1_get_data(void)
{
	if (u1_recv_flag == 0U)
	{
		return NULL;
	}

	u1_recv_flag = 0U;
	u1_recv_length = 0U;
	return u1_recv_buff;
}

uint16_t uart1_get_data_length(void)
{
	return u1_recv_frame_length;
}

uint8_t uart1_get_overflow(void)
{
	return u1_recv_overflow;
}

uint16_t uart1_read(uint8_t *data, uint16_t capacity, uint8_t *overflow)
{
	uint16_t length;
	uint16_t copy_length;
	uint8_t truncated;

	if (overflow != NULL)
	{
		*overflow = 0U;
	}
	if (data == NULL || capacity == 0U || u1_recv_flag == 0U)
	{
		return 0U;
	}

	length = u1_recv_frame_length;
	copy_length = length;
	truncated = u1_recv_overflow;
	if (copy_length >= capacity)
	{
		copy_length = capacity - 1U;
		truncated = 1U;
	}

	memcpy(data, u1_recv_buff, copy_length);
	data[copy_length] = '\0';
	if (overflow != NULL)
	{
		*overflow = truncated;
	}

	uart1_receive_clear();
	return copy_length;
}

void uart1_irq_handler(void)
{
	if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
	{
		uint8_t value = (uint8_t)USART_ReceiveData(USART1);

		if (u1_recv_flag == 0U)
		{
			if (u1_recv_length == 0U)
			{
				u1_recv_overflow = 0U;
			}
			if (u1_recv_length < USART1_RECEIVE_LENGTH - 1U)
			{
				u1_recv_buff[u1_recv_length++] = value;
			}
			else
			{
				u1_recv_overflow = 1U;
			}
		}
		else
		{
			u1_recv_overflow = 1U;
		}

		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}

	if (USART_GetITStatus(USART1, USART_IT_IDLE) == SET)
	{
		volatile uint32_t status = USART1->SR;
		status = USART1->DR;
		(void)status;

		if (u1_recv_flag == 0U)
		{
			u1_recv_buff[u1_recv_length] = '\0';
			u1_recv_frame_length = u1_recv_length;
			u1_recv_flag = 1U;
		}
	}
}
