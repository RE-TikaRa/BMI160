#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "stm32f10x.h"

typedef enum
{
	BSP_I2C_OK = 0,
	BSP_I2C_ERROR_ARGUMENT,
	BSP_I2C_ERROR_TIMEOUT,
	BSP_I2C_ERROR_NACK,
	BSP_I2C_ERROR_BUS,
	BSP_I2C_ERROR_ARBITRATION,
	BSP_I2C_ERROR_OVERRUN
} bsp_i2c_status_t;

void i2c1_init(void);
bsp_i2c_status_t i2c1_write_registers(uint8_t address, uint8_t reg, const uint8_t *data, uint16_t length);
bsp_i2c_status_t i2c1_read_registers(uint8_t address, uint8_t reg, uint8_t *data, uint16_t length);

#endif
