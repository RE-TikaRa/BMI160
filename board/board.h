#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"

#define BOARD_LED_GPIO       GPIOC
#define BOARD_LED_GPIO_CLOCK RCC_APB2Periph_GPIOC
#define BOARD_LED_PIN        GPIO_Pin_13
#define BOARD_LED_ON_STATE   Bit_RESET
#define BOARD_LED_OFF_STATE  Bit_SET

void board_init(void);
void board_led_init(void);
void board_led_write(BitAction state);
void board_led_on(void);
void board_led_off(void);
void board_led_toggle(void);

void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

void delay_1us(uint32_t us);
void delay_1ms(uint32_t ms);

#endif
