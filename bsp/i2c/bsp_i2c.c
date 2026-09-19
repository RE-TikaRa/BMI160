#include "bsp_i2c.h"
#include <stddef.h>

#define I2C1_TRANSFER_TIMEOUT 100000U

static bsp_i2c_status_t i2c1_get_error(void)
{
	uint16_t status = I2C1->SR1;

	if ((status & I2C_SR1_AF) != 0U)
	{
		return BSP_I2C_ERROR_NACK;
	}
	if ((status & I2C_SR1_BERR) != 0U)
	{
		return BSP_I2C_ERROR_BUS;
	}
	if ((status & I2C_SR1_ARLO) != 0U)
	{
		return BSP_I2C_ERROR_ARBITRATION;
	}
	if ((status & I2C_SR1_OVR) != 0U)
	{
		return BSP_I2C_ERROR_OVERRUN;
	}

	return BSP_I2C_OK;
}

static bsp_i2c_status_t i2c1_wait_sr1(uint16_t flag)
{
	uint32_t timeout = I2C1_TRANSFER_TIMEOUT;
	bsp_i2c_status_t status;

	while ((I2C1->SR1 & flag) == 0U)
	{
		status = i2c1_get_error();
		if (status != BSP_I2C_OK)
		{
			return status;
		}
		if (timeout-- == 0U)
		{
			return BSP_I2C_ERROR_TIMEOUT;
		}
	}

	return BSP_I2C_OK;
}

static bsp_i2c_status_t i2c1_wait_idle(void)
{
	uint32_t timeout = I2C1_TRANSFER_TIMEOUT;

	while ((I2C1->SR2 & I2C_SR2_BUSY) != 0U)
	{
		if (timeout-- == 0U)
		{
			return BSP_I2C_ERROR_TIMEOUT;
		}
	}

	return BSP_I2C_OK;
}

static void i2c1_clear_addr(void)
{
	volatile uint16_t status;

	status = I2C1->SR1;
	status = I2C1->SR2;
	(void)status;
}

static bsp_i2c_status_t i2c1_abort(bsp_i2c_status_t status)
{
	I2C_GenerateSTOP(I2C1, ENABLE);
	I2C_AcknowledgeConfig(I2C1, ENABLE);
	I2C_NACKPositionConfig(I2C1, I2C_NACKPosition_Current);
	I2C_ClearFlag(I2C1, I2C_FLAG_BERR | I2C_FLAG_ARLO | I2C_FLAG_AF | I2C_FLAG_OVR);
	return status;
}

static bsp_i2c_status_t i2c1_start(uint8_t address, uint8_t direction)
{
	bsp_i2c_status_t status;

	I2C_GenerateSTART(I2C1, ENABLE);
	status = i2c1_wait_sr1(I2C_SR1_SB);
	if (status != BSP_I2C_OK)
	{
		return status;
	}

	I2C_Send7bitAddress(I2C1, (uint8_t)(address << 1), direction);
	return i2c1_wait_sr1(I2C_SR1_ADDR);
}

void i2c1_init(void)
{
	GPIO_InitTypeDef gpio;
	I2C_InitTypeDef i2c;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

	GPIO_StructInit(&gpio);
	gpio.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	gpio.GPIO_Mode = GPIO_Mode_AF_OD;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio);

	I2C_DeInit(I2C1);
	I2C_StructInit(&i2c);
	i2c.I2C_ClockSpeed = 100000U;
	i2c.I2C_Mode = I2C_Mode_I2C;
	i2c.I2C_DutyCycle = I2C_DutyCycle_2;
	i2c.I2C_OwnAddress1 = 0U;
	i2c.I2C_Ack = I2C_Ack_Enable;
	i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_Init(I2C1, &i2c);
	I2C_Cmd(I2C1, ENABLE);
}

bsp_i2c_status_t i2c1_write_registers(uint8_t address, uint8_t reg, const uint8_t *data, uint16_t length)
{
	bsp_i2c_status_t status;
	uint16_t index;

	if (address > 0x7FU || data == NULL || length == 0U)
	{
		return BSP_I2C_ERROR_ARGUMENT;
	}

	status = i2c1_wait_idle();
	if (status != BSP_I2C_OK)
	{
		return i2c1_abort(status);
	}

	status = i2c1_start(address, I2C_Direction_Transmitter);
	if (status != BSP_I2C_OK)
	{
		return i2c1_abort(status);
	}
	i2c1_clear_addr();

	I2C_SendData(I2C1, reg);
	status = i2c1_wait_sr1(I2C_SR1_BTF);
	if (status != BSP_I2C_OK)
	{
		return i2c1_abort(status);
	}

	for (index = 0U; index < length; index++)
	{
		I2C_SendData(I2C1, data[index]);
		status = i2c1_wait_sr1(I2C_SR1_BTF);
		if (status != BSP_I2C_OK)
		{
			return i2c1_abort(status);
		}
	}

	I2C_GenerateSTOP(I2C1, ENABLE);
	return BSP_I2C_OK;
}

bsp_i2c_status_t i2c1_read_registers(uint8_t address, uint8_t reg, uint8_t *data, uint16_t length)
{
	bsp_i2c_status_t status;
	uint16_t remaining;
	uint16_t index = 0U;
	uint32_t primask;

	if (address > 0x7FU || data == NULL || length == 0U)
	{
		return BSP_I2C_ERROR_ARGUMENT;
	}

	I2C_AcknowledgeConfig(I2C1, ENABLE);
	I2C_NACKPositionConfig(I2C1, I2C_NACKPosition_Current);
	status = i2c1_wait_idle();
	if (status != BSP_I2C_OK)
	{
		return i2c1_abort(status);
	}

	status = i2c1_start(address, I2C_Direction_Transmitter);
	if (status != BSP_I2C_OK)
	{
		return i2c1_abort(status);
	}
	i2c1_clear_addr();

	I2C_SendData(I2C1, reg);
	status = i2c1_wait_sr1(I2C_SR1_BTF);
	if (status != BSP_I2C_OK)
	{
		return i2c1_abort(status);
	}

	status = i2c1_start(address, I2C_Direction_Receiver);
	if (status != BSP_I2C_OK)
	{
		return i2c1_abort(status);
	}

	remaining = length;
	if (remaining == 1U)
	{
		primask = __get_PRIMASK();
		__disable_irq();
		I2C_AcknowledgeConfig(I2C1, DISABLE);
		i2c1_clear_addr();
		I2C_GenerateSTOP(I2C1, ENABLE);
		if (primask == 0U)
		{
			__enable_irq();
		}

		status = i2c1_wait_sr1(I2C_SR1_RXNE);
		if (status != BSP_I2C_OK)
		{
			return i2c1_abort(status);
		}
		data[0] = I2C_ReceiveData(I2C1);
	}
	else if (remaining == 2U)
	{
		I2C_NACKPositionConfig(I2C1, I2C_NACKPosition_Next);
		primask = __get_PRIMASK();
		__disable_irq();
		I2C_AcknowledgeConfig(I2C1, DISABLE);
		i2c1_clear_addr();
		if (primask == 0U)
		{
			__enable_irq();
		}

		status = i2c1_wait_sr1(I2C_SR1_BTF);
		if (status != BSP_I2C_OK)
		{
			return i2c1_abort(status);
		}

		primask = __get_PRIMASK();
		__disable_irq();
		I2C_GenerateSTOP(I2C1, ENABLE);
		data[0] = I2C_ReceiveData(I2C1);
		data[1] = I2C_ReceiveData(I2C1);
		if (primask == 0U)
		{
			__enable_irq();
		}
	}
	else
	{
		i2c1_clear_addr();
		while (remaining > 3U)
		{
			status = i2c1_wait_sr1(I2C_SR1_RXNE);
			if (status != BSP_I2C_OK)
			{
				return i2c1_abort(status);
			}
			data[index++] = I2C_ReceiveData(I2C1);
			remaining--;
		}

		status = i2c1_wait_sr1(I2C_SR1_BTF);
		if (status != BSP_I2C_OK)
		{
			return i2c1_abort(status);
		}

		primask = __get_PRIMASK();
		__disable_irq();
		I2C_AcknowledgeConfig(I2C1, DISABLE);
		data[index++] = I2C_ReceiveData(I2C1);
		I2C_GenerateSTOP(I2C1, ENABLE);
		data[index++] = I2C_ReceiveData(I2C1);
		if (primask == 0U)
		{
			__enable_irq();
		}

		status = i2c1_wait_sr1(I2C_SR1_RXNE);
		if (status != BSP_I2C_OK)
		{
			return i2c1_abort(status);
		}
		data[index] = I2C_ReceiveData(I2C1);
	}

	I2C_AcknowledgeConfig(I2C1, ENABLE);
	I2C_NACKPositionConfig(I2C1, I2C_NACKPosition_Current);
	return BSP_I2C_OK;
}
