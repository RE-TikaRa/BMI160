#include "board.h"

static uint32_t systick_ticks_per_us;

void board_init(void)
{
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
	systick_ticks_per_us = SystemCoreClock / 8000000U;
}

void board_led_init(void)
{
	GPIO_InitTypeDef gpio;

	RCC_APB2PeriphClockCmd(BOARD_LED_GPIO_CLOCK, ENABLE);
	GPIO_StructInit(&gpio);
	gpio.GPIO_Pin = BOARD_LED_PIN;
	gpio.GPIO_Mode = GPIO_Mode_Out_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BOARD_LED_GPIO, &gpio);
	board_led_off();
}

void board_led_write(BitAction state)
{
	GPIO_WriteBit(BOARD_LED_GPIO, BOARD_LED_PIN, state);
}

void board_led_on(void)
{
	board_led_write(BOARD_LED_ON_STATE);
}

void board_led_off(void)
{
	board_led_write(BOARD_LED_OFF_STATE);
}

void board_led_toggle(void)
{
	BOARD_LED_GPIO->ODR ^= BOARD_LED_PIN;
}

void delay_us(uint32_t us)
{
	uint32_t status;
	uint32_t count;

	if (us == 0U || systick_ticks_per_us == 0U)
	{
		return;
	}

	count = us * systick_ticks_per_us;
	if (count < 11U)
	{
		count = 11U;
	}

	SysTick->LOAD = count - ((us == 1U) ? 8U : 10U);
	SysTick->VAL = 0U;
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;

	do
	{
		status = SysTick->CTRL;
	} while ((status & SysTick_CTRL_ENABLE_Msk) != 0U &&
	         (status & SysTick_CTRL_COUNTFLAG_Msk) == 0U);

	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
	SysTick->VAL = 0U;
}

void delay_ms(uint32_t ms)
{
	delay_us(ms * 1000U);
}

void delay_1us(uint32_t us)
{
	delay_us(us);
}

void delay_1ms(uint32_t ms)
{
	delay_ms(ms);
}
