#include "bsp_bmi160.h"
#include "board.h"
#include "bsp_i2c.h"
#include <string.h>

#define BMI160_REG_CHIP_ID       0x00U
#define BMI160_REG_ERROR         0x02U
#define BMI160_REG_PMU_STATUS    0x03U
#define BMI160_REG_GYRO_DATA     0x0CU
#define BMI160_REG_STATUS        0x1BU
#define BMI160_REG_INT_STATUS    0x1CU
#define BMI160_REG_TEMPERATURE   0x20U
#define BMI160_REG_FIFO_LENGTH   0x22U
#define BMI160_REG_FIFO_DATA     0x24U
#define BMI160_REG_ACCEL_CONFIG  0x40U
#define BMI160_REG_ACCEL_RANGE   0x41U
#define BMI160_REG_GYRO_CONFIG   0x42U
#define BMI160_REG_GYRO_RANGE    0x43U
#define BMI160_REG_FIFO_DOWN     0x45U
#define BMI160_REG_FIFO_CONFIG_0 0x46U
#define BMI160_REG_FIFO_CONFIG_1 0x47U
#define BMI160_REG_INT_ENABLE_0  0x50U
#define BMI160_REG_INT_ENABLE_1  0x51U
#define BMI160_REG_INT_ENABLE_2  0x52U
#define BMI160_REG_INT_OUT_CTRL  0x53U
#define BMI160_REG_INT_LATCH     0x54U
#define BMI160_REG_INT_MAP_0     0x55U
#define BMI160_REG_INT_MAP_1     0x56U
#define BMI160_REG_INT_MAP_2     0x57U
#define BMI160_REG_INT_DATA_0    0x58U
#define BMI160_REG_INT_DATA_1    0x59U
#define BMI160_REG_INT_LOWHIGH_0 0x5AU
#define BMI160_REG_INT_LOWHIGH_2 0x5CU
#define BMI160_REG_INT_MOTION_0  0x5FU
#define BMI160_REG_INT_MOTION_1  0x60U
#define BMI160_REG_INT_MOTION_2  0x61U
#define BMI160_REG_INT_MOTION_3  0x62U
#define BMI160_REG_INT_TAP_0     0x63U
#define BMI160_REG_INT_ORIENT_0  0x65U
#define BMI160_REG_INT_FLAT_0    0x67U
#define BMI160_REG_FOC_CONFIG    0x69U
#define BMI160_REG_SELF_TEST     0x6DU
#define BMI160_REG_OFFSET_0      0x71U
#define BMI160_REG_OFFSET_6      0x77U
#define BMI160_REG_STEP_COUNT    0x78U
#define BMI160_REG_STEP_CONFIG_0 0x7AU
#define BMI160_REG_STEP_CONFIG_1 0x7BU
#define BMI160_REG_COMMAND       0x7EU

#define BMI160_COMMAND_START_FOC  0x03U
#define BMI160_COMMAND_FIFO_FLUSH 0xB0U
#define BMI160_COMMAND_STEP_RESET 0xB2U
#define BMI160_COMMAND_SOFT_RESET 0xB6U

#define BMI160_STATUS_GYRO_SELF_TEST 0x02U
#define BMI160_STATUS_FOC_DONE       0x08U

#define BMI160_INT_MOTION_SOURCE 0x80U
#define BMI160_INT_LOW_HIGH_SOURCE 0x80U

static const uint8_t bmi160_interrupt_map_mask[] = {
	0x04U, 0x04U, 0x01U, 0x10U, 0x20U, 0x40U, 0x80U,
	0x02U, 0x01U, 0x08U, 0x08U, 0x02U, 0x04U
};

static bmi160_status_t bmi160_from_i2c_status(bsp_i2c_status_t status)
{
	if (status == BSP_I2C_OK)
	{
		return BMI160_OK;
	}
	if (status == BSP_I2C_ERROR_TIMEOUT)
	{
		return BMI160_ERROR_TIMEOUT;
	}

	return BMI160_ERROR_I2C;
}

static int16_t bmi160_parse_int16(const uint8_t *data)
{
	return (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static void bmi160_parse_vector(const uint8_t *data, bmi160_vector_t *vector)
{
	vector->x = bmi160_parse_int16(&data[0]);
	vector->y = bmi160_parse_int16(&data[2]);
	vector->z = bmi160_parse_int16(&data[4]);
}

static uint8_t bmi160_valid_config(const bmi160_config_t *config)
{
	if (config == NULL)
	{
		return 0U;
	}
	if (config->accel_odr < BMI160_ACCEL_ODR_0_78HZ || config->accel_odr > BMI160_ACCEL_ODR_1600HZ)
	{
		return 0U;
	}
	if (config->accel_bandwidth > BMI160_ACCEL_BW_RES_AVG128)
	{
		return 0U;
	}
	if (config->accel_range != BMI160_ACCEL_RANGE_2G &&
		config->accel_range != BMI160_ACCEL_RANGE_4G &&
		config->accel_range != BMI160_ACCEL_RANGE_8G &&
		config->accel_range != BMI160_ACCEL_RANGE_16G)
	{
		return 0U;
	}
	if (config->gyro_odr < BMI160_GYRO_ODR_25HZ || config->gyro_odr > BMI160_GYRO_ODR_3200HZ)
	{
		return 0U;
	}
	if (config->gyro_bandwidth > BMI160_GYRO_BW_NORMAL)
	{
		return 0U;
	}
	if (config->gyro_range > BMI160_GYRO_RANGE_125DPS)
	{
		return 0U;
	}

	return 1U;
}

static bmi160_status_t bmi160_update_register(const bmi160_t *device, uint8_t reg, uint8_t mask, uint8_t value)
{
	bmi160_status_t status;
	uint8_t data;

	status = bmi160_read_registers(device, reg, &data, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}

	data = (uint8_t)((data & (uint8_t)~mask) | (value & mask));
	return bmi160_write_registers(device, reg, &data, 1U);
}

static bmi160_status_t bmi160_route_interrupt(const bmi160_t *device,
		bmi160_interrupt_t type, bmi160_interrupt_pin_t pin)
{
	bmi160_status_t status;
	uint8_t mask;
	uint8_t map[3];
	uint8_t data;

	if (type > BMI160_INTERRUPT_FIFO_WATERMARK || pin > BMI160_INTERRUPT_BOTH)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	mask = bmi160_interrupt_map_mask[type];
	if (type >= BMI160_INTERRUPT_DATA_READY)
	{
		status = bmi160_read_registers(device, BMI160_REG_INT_MAP_1, &data, 1U);
		if (status != BMI160_OK)
		{
			return status;
		}

		data &= (uint8_t)~(mask | (uint8_t)(mask << 4));
		if ((pin & BMI160_INTERRUPT_1) != 0U)
		{
			data |= (uint8_t)(mask << 4);
		}
		if ((pin & BMI160_INTERRUPT_2) != 0U)
		{
			data |= mask;
		}
		return bmi160_write_registers(device, BMI160_REG_INT_MAP_1, &data, 1U);
	}

	status = bmi160_read_registers(device, BMI160_REG_INT_MAP_0, map, 3U);
	if (status != BMI160_OK)
	{
		return status;
	}

	map[0] &= (uint8_t)~mask;
	map[2] &= (uint8_t)~mask;
	if ((pin & BMI160_INTERRUPT_1) != 0U)
	{
		map[0] |= mask;
	}
	if ((pin & BMI160_INTERRUPT_2) != 0U)
	{
		map[2] |= mask;
	}

	return bmi160_write_registers(device, BMI160_REG_INT_MAP_0, map, 3U);
}

static uint16_t bmi160_accel_full_scale(const bmi160_t *device)
{
	switch (device->config.accel_range)
	{
		case BMI160_ACCEL_RANGE_4G:
			return 4U;
		case BMI160_ACCEL_RANGE_8G:
			return 8U;
		case BMI160_ACCEL_RANGE_16G:
			return 16U;
		default:
			return 2U;
	}
}

static uint16_t bmi160_gyro_full_scale(const bmi160_t *device)
{
	switch (device->config.gyro_range)
	{
		case BMI160_GYRO_RANGE_1000DPS:
			return 1000U;
		case BMI160_GYRO_RANGE_500DPS:
			return 500U;
		case BMI160_GYRO_RANGE_250DPS:
			return 250U;
		case BMI160_GYRO_RANGE_125DPS:
			return 125U;
		default:
			return 2000U;
	}
}

bmi160_config_t bmi160_default_config(void)
{
	bmi160_config_t config;

	config.accel_odr = BMI160_ACCEL_ODR_100HZ;
	config.accel_bandwidth = BMI160_ACCEL_BW_NORMAL;
	config.accel_range = BMI160_ACCEL_RANGE_2G;
	config.gyro_odr = BMI160_GYRO_ODR_100HZ;
	config.gyro_bandwidth = BMI160_GYRO_BW_NORMAL;
	config.gyro_range = BMI160_GYRO_RANGE_500DPS;
	return config;
}

bmi160_status_t bmi160_read_registers(const bmi160_t *device, uint8_t reg, uint8_t *data, uint16_t length)
{
	if (device == NULL || data == NULL || length == 0U)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	return bmi160_from_i2c_status(i2c1_read_registers(device->address, reg, data, length));
}

bmi160_status_t bmi160_write_registers(const bmi160_t *device, uint8_t reg, const uint8_t *data, uint16_t length)
{
	bmi160_status_t status;
	uint16_t index;

	if (device == NULL || data == NULL || length == 0U)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	for (index = 0U; index < length; index++)
	{
		status = bmi160_from_i2c_status(i2c1_write_registers(device->address,
				(uint8_t)(reg + index), &data[index], 1U));
		if (status != BMI160_OK)
		{
			return status;
		}
		delay_ms(1U);
	}

	return BMI160_OK;
}

bmi160_status_t bmi160_soft_reset(bmi160_t *device)
{
	bmi160_status_t status;
	uint8_t command = BMI160_COMMAND_SOFT_RESET;

	if (device == NULL)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	status = bmi160_write_registers(device, BMI160_REG_COMMAND, &command, 1U);
	delay_ms(1U);
	if (status == BMI160_OK)
	{
		device->fifo_sensors = (uint8_t)BMI160_FIFO_NONE;
		device->fifo_header = 0U;
		device->fifo_sensor_time = 0U;
	}
	return status;
}

bmi160_status_t bmi160_set_accel_power(bmi160_t *device, bmi160_accel_power_t power)
{
	bmi160_status_t status;
	uint8_t command = (uint8_t)power;
	uint8_t undersampling;

	if (device == NULL || (power != BMI160_ACCEL_SUSPEND &&
		power != BMI160_ACCEL_NORMAL && power != BMI160_ACCEL_LOW_POWER))
	{
		return BMI160_ERROR_ARGUMENT;
	}

	undersampling = (power == BMI160_ACCEL_LOW_POWER) ? 0x80U : 0x00U;
	status = bmi160_update_register(device, BMI160_REG_ACCEL_CONFIG, 0x80U, undersampling);
	if (status != BMI160_OK)
	{
		return status;
	}

	status = bmi160_write_registers(device, BMI160_REG_COMMAND, &command, 1U);
	delay_ms(5U);
	return status;
}

bmi160_status_t bmi160_set_gyro_power(bmi160_t *device, bmi160_gyro_power_t power)
{
	bmi160_status_t status;
	uint8_t command = (uint8_t)power;

	if (device == NULL || (power != BMI160_GYRO_SUSPEND &&
		power != BMI160_GYRO_NORMAL && power != BMI160_GYRO_FAST_STARTUP))
	{
		return BMI160_ERROR_ARGUMENT;
	}

	status = bmi160_write_registers(device, BMI160_REG_COMMAND, &command, 1U);
	delay_ms(80U);
	return status;
}

bmi160_status_t bmi160_get_power_status(const bmi160_t *device, bmi160_power_status_t *status)
{
	bmi160_status_t result;
	uint8_t data;

	if (status == NULL)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	result = bmi160_read_registers(device, BMI160_REG_PMU_STATUS, &data, 1U);
	if (result == BMI160_OK)
	{
		status->accel = (uint8_t)((data >> 4) & 0x03U);
		status->gyro = (uint8_t)((data >> 2) & 0x03U);
	}
	return result;
}

bmi160_status_t bmi160_apply_config(bmi160_t *device, const bmi160_config_t *config)
{
	bmi160_status_t status;
	uint8_t data;
	uint8_t error;

	if (device == NULL || bmi160_valid_config(config) == 0U)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	data = (uint8_t)(((uint8_t)config->accel_bandwidth << 4) | (uint8_t)config->accel_odr);
	status = bmi160_write_registers(device, BMI160_REG_ACCEL_CONFIG, &data, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}

	data = (uint8_t)config->accel_range;
	status = bmi160_write_registers(device, BMI160_REG_ACCEL_RANGE, &data, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}

	data = (uint8_t)(((uint8_t)config->gyro_bandwidth << 4) | (uint8_t)config->gyro_odr);
	status = bmi160_write_registers(device, BMI160_REG_GYRO_CONFIG, &data, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}

	data = (uint8_t)config->gyro_range;
	status = bmi160_write_registers(device, BMI160_REG_GYRO_RANGE, &data, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}

	status = bmi160_set_accel_power(device, BMI160_ACCEL_NORMAL);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_set_gyro_power(device, BMI160_GYRO_NORMAL);
	if (status != BMI160_OK)
	{
		return status;
	}

	status = bmi160_read_error(device, &error);
	if (status != BMI160_OK)
	{
		return status;
	}
	if ((error & 0x5FU) != 0U)
	{
		return BMI160_ERROR_CONFIG;
	}

	device->config = *config;
	return BMI160_OK;
}

bmi160_status_t bmi160_init(bmi160_t *device, uint8_t address, const bmi160_config_t *config)
{
	bmi160_status_t status;
	bmi160_config_t requested_config;
	bmi160_power_status_t power;
	uint8_t chip_id;

	if (device == NULL || config == NULL ||
		(address != BMI160_I2C_ADDRESS_LOW && address != BMI160_I2C_ADDRESS_HIGH))
	{
		return BMI160_ERROR_ARGUMENT;
	}

	requested_config = *config;
	memset(device, 0, sizeof *device);
	device->address = address;
	delay_ms(10U);
	status = bmi160_read_registers(device, BMI160_REG_CHIP_ID, &chip_id, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	if (chip_id != BMI160_CHIP_ID)
	{
		return BMI160_ERROR_DEVICE;
	}

	device->chip_id = chip_id;
	status = bmi160_soft_reset(device);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_read_registers(device, BMI160_REG_CHIP_ID, &chip_id, 1U);
	if (status != BMI160_OK || chip_id != BMI160_CHIP_ID)
	{
		return (status == BMI160_OK) ? BMI160_ERROR_DEVICE : status;
	}

	status = bmi160_apply_config(device, &requested_config);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_get_power_status(device, &power);
	if (status != BMI160_OK)
	{
		return status;
	}
	if (power.accel != 0x01U || power.gyro != 0x01U)
	{
		return BMI160_ERROR_NOT_READY;
	}

	return BMI160_OK;
}

bmi160_status_t bmi160_read_sample(const bmi160_t *device, bmi160_sample_t *sample)
{
	bmi160_status_t status;
	uint8_t data[15];

	if (sample == NULL)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	status = bmi160_read_registers(device, BMI160_REG_GYRO_DATA, data, sizeof data);
	if (status != BMI160_OK)
	{
		return status;
	}

	bmi160_parse_vector(&data[0], &sample->gyro);
	bmi160_parse_vector(&data[6], &sample->accel);
	sample->sensor_time = (uint32_t)data[12] |
		((uint32_t)data[13] << 8) | ((uint32_t)data[14] << 16);
	return BMI160_OK;
}

bmi160_status_t bmi160_read_temperature(const bmi160_t *device, int32_t *temperature_millicelsius)
{
	bmi160_status_t status;
	uint8_t data[2];
	int16_t raw;

	if (temperature_millicelsius == NULL)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	status = bmi160_read_registers(device, BMI160_REG_TEMPERATURE, data, sizeof data);
	if (status != BMI160_OK)
	{
		return status;
	}
	raw = bmi160_parse_int16(data);
	if ((uint16_t)raw == 0x8000U)
	{
		return BMI160_ERROR_NOT_READY;
	}

	*temperature_millicelsius = 23000L + ((int32_t)raw * 1000L) / 512L;
	return BMI160_OK;
}

bmi160_status_t bmi160_read_error(const bmi160_t *device, uint8_t *error)
{
	if (error == NULL)
	{
		return BMI160_ERROR_ARGUMENT;
	}
	return bmi160_read_registers(device, BMI160_REG_ERROR, error, 1U);
}

bmi160_status_t bmi160_read_interrupt_status(const bmi160_t *device, uint32_t *status)
{
	bmi160_status_t result;
	uint8_t data[4];

	if (status == NULL)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	result = bmi160_read_registers(device, BMI160_REG_INT_STATUS, data, sizeof data);
	if (result == BMI160_OK)
	{
		*status = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
			((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
	}
	return result;
}

void bmi160_scale_sample(const bmi160_t *device, const bmi160_sample_t *sample, bmi160_scaled_sample_t *scaled)
{
	int64_t accel_scale;
	int64_t gyro_scale;

	if (device == NULL || sample == NULL || scaled == NULL)
	{
		return;
	}

	accel_scale = (int64_t)bmi160_accel_full_scale(device) * 1000LL;
	gyro_scale = (int64_t)bmi160_gyro_full_scale(device) * 1000LL;
	scaled->accel_mg.x = (int32_t)(((int64_t)sample->accel.x * accel_scale) / 32768LL);
	scaled->accel_mg.y = (int32_t)(((int64_t)sample->accel.y * accel_scale) / 32768LL);
	scaled->accel_mg.z = (int32_t)(((int64_t)sample->accel.z * accel_scale) / 32768LL);
	scaled->gyro_mdps.x = (int32_t)(((int64_t)sample->gyro.x * gyro_scale) / 32768LL);
	scaled->gyro_mdps.y = (int32_t)(((int64_t)sample->gyro.y * gyro_scale) / 32768LL);
	scaled->gyro_mdps.z = (int32_t)(((int64_t)sample->gyro.z * gyro_scale) / 32768LL);
	scaled->sensor_time = sample->sensor_time;
}

bmi160_status_t bmi160_configure_interrupt_pin(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_interrupt_pin_config_t *config)
{
	bmi160_status_t status;
	uint8_t output;
	uint8_t latch;
	uint8_t value;

	if (device == NULL || config == NULL || pin == BMI160_INTERRUPT_NONE ||
		pin > BMI160_INTERRUPT_BOTH || config->latch > 0x0FU)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	status = bmi160_read_registers(device, BMI160_REG_INT_OUT_CTRL, &output, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_read_registers(device, BMI160_REG_INT_LATCH, &latch, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}

	if ((pin & BMI160_INTERRUPT_1) != 0U)
	{
		value = 0x08U |
			(uint8_t)((config->open_drain & 0x01U) << 2) |
			(uint8_t)((config->active_high & 0x01U) << 1) |
			(config->edge_triggered & 0x01U);
		output = (uint8_t)((output & 0xF0U) | value);
		latch = (uint8_t)((latch & (uint8_t)~0x10U) |
			(uint8_t)((config->input_enable & 0x01U) << 4));
	}
	if ((pin & BMI160_INTERRUPT_2) != 0U)
	{
		value = 0x80U |
			(uint8_t)((config->open_drain & 0x01U) << 6) |
			(uint8_t)((config->active_high & 0x01U) << 5) |
			(uint8_t)((config->edge_triggered & 0x01U) << 4);
		output = (uint8_t)((output & 0x0FU) | value);
		latch = (uint8_t)((latch & (uint8_t)~0x20U) |
			(uint8_t)((config->input_enable & 0x01U) << 5));
	}
	latch = (uint8_t)((latch & 0xF0U) | config->latch);

	status = bmi160_write_registers(device, BMI160_REG_INT_OUT_CTRL, &output, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	return bmi160_write_registers(device, BMI160_REG_INT_LATCH, &latch, 1U);
}

bmi160_status_t bmi160_configure_any_motion(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_motion_config_t *config)
{
	bmi160_status_t status;
	uint8_t axes;
	uint8_t motion;

	if (config == NULL || pin > BMI160_INTERRUPT_BOTH || config->duration > 3U)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	axes = 0U;
	if (pin != BMI160_INTERRUPT_NONE)
	{
		axes = (uint8_t)((config->x & 0x01U) |
			((config->y & 0x01U) << 1) | ((config->z & 0x01U) << 2));
	}
	status = bmi160_update_register(device, BMI160_REG_INT_ENABLE_0, 0x07U, axes);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_update_register(device, BMI160_REG_INT_MOTION_3, 0x02U, 0U);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_update_register(device, BMI160_REG_INT_DATA_1,
		BMI160_INT_MOTION_SOURCE, (uint8_t)((config->unfiltered & 0x01U) << 7));
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_read_registers(device, BMI160_REG_INT_MOTION_0, &motion, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	motion = (uint8_t)((motion & 0xFCU) | config->duration);
	status = bmi160_write_registers(device, BMI160_REG_INT_MOTION_0, &motion, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_write_registers(device, BMI160_REG_INT_MOTION_1, &config->threshold, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	return bmi160_route_interrupt(device, BMI160_INTERRUPT_ANY_MOTION, pin);
}

bmi160_status_t bmi160_configure_significant_motion(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_significant_motion_config_t *config)
{
	bmi160_status_t status;
	uint8_t motion;
	uint8_t enable;

	if (config == NULL || pin > BMI160_INTERRUPT_BOTH || config->skip > 3U || config->proof > 3U)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	enable = (pin == BMI160_INTERRUPT_NONE) ? 0U : 0x07U;
	status = bmi160_update_register(device, BMI160_REG_INT_ENABLE_0, 0x07U, enable);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_update_register(device, BMI160_REG_INT_DATA_1,
		BMI160_INT_MOTION_SOURCE, (uint8_t)((config->unfiltered & 0x01U) << 7));
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_write_registers(device, BMI160_REG_INT_MOTION_1, &config->threshold, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_read_registers(device, BMI160_REG_INT_MOTION_3, &motion, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	motion = (uint8_t)((motion & (uint8_t)~0x3EU) |
		(uint8_t)((config->skip & 0x03U) << 2) |
		(uint8_t)((config->proof & 0x03U) << 4));
	if (pin != BMI160_INTERRUPT_NONE)
	{
		motion |= 0x02U;
	}
	status = bmi160_write_registers(device, BMI160_REG_INT_MOTION_3, &motion, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	return bmi160_route_interrupt(device, BMI160_INTERRUPT_SIGNIFICANT_MOTION, pin);
}

bmi160_status_t bmi160_configure_no_motion(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_motion_config_t *config)
{
	bmi160_status_t status;
	uint8_t axes;
	uint8_t motion;

	if (config == NULL || pin > BMI160_INTERRUPT_BOTH || config->duration > 63U)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	axes = 0U;
	if (pin != BMI160_INTERRUPT_NONE)
	{
		axes = (uint8_t)((config->x & 0x01U) |
			((config->y & 0x01U) << 1) | ((config->z & 0x01U) << 2));
	}
	status = bmi160_update_register(device, BMI160_REG_INT_ENABLE_2, 0x07U, axes);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_update_register(device, BMI160_REG_INT_DATA_1,
		BMI160_INT_MOTION_SOURCE, (uint8_t)((config->unfiltered & 0x01U) << 7));
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_read_registers(device, BMI160_REG_INT_MOTION_0, &motion, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	motion = (uint8_t)((motion & 0x03U) | (uint8_t)(config->duration << 2));
	status = bmi160_write_registers(device, BMI160_REG_INT_MOTION_0, &motion, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_write_registers(device, BMI160_REG_INT_MOTION_2, &config->threshold, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_update_register(device, BMI160_REG_INT_MOTION_3, 0x01U, 0x01U);
	if (status != BMI160_OK)
	{
		return status;
	}
	return bmi160_route_interrupt(device, BMI160_INTERRUPT_NO_MOTION, pin);
}

bmi160_status_t bmi160_configure_tap(const bmi160_t *device, bmi160_interrupt_pin_t pin,
		bmi160_interrupt_t type, const bmi160_tap_config_t *config)
{
	bmi160_status_t status;
	uint8_t enable_mask;
	uint8_t tap[2];

	if (config == NULL || pin > BMI160_INTERRUPT_BOTH ||
		(type != BMI160_INTERRUPT_SINGLE_TAP && type != BMI160_INTERRUPT_DOUBLE_TAP) ||
		config->duration > 7U || config->threshold > 31U)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	enable_mask = (type == BMI160_INTERRUPT_SINGLE_TAP) ? 0x20U : 0x10U;
	status = bmi160_update_register(device, BMI160_REG_INT_ENABLE_0, enable_mask,
		(pin == BMI160_INTERRUPT_NONE) ? 0U : enable_mask);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_update_register(device, BMI160_REG_INT_DATA_0, 0x08U,
		(uint8_t)((config->unfiltered & 0x01U) << 3));
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_read_registers(device, BMI160_REG_INT_TAP_0, tap, sizeof tap);
	if (status != BMI160_OK)
	{
		return status;
	}
	tap[0] = (uint8_t)((tap[0] & 0x38U) | (config->duration & 0x07U) |
		(uint8_t)((config->shock & 0x01U) << 6) |
		(uint8_t)((config->quiet & 0x01U) << 7));
	tap[1] = (uint8_t)((tap[1] & 0xE0U) | config->threshold);
	status = bmi160_write_registers(device, BMI160_REG_INT_TAP_0, tap, sizeof tap);
	if (status != BMI160_OK)
	{
		return status;
	}
	return bmi160_route_interrupt(device, type, pin);
}

bmi160_status_t bmi160_configure_orientation(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_orientation_config_t *config)
{
	bmi160_status_t status;
	uint8_t data[2];

	if (config == NULL || pin > BMI160_INTERRUPT_BOTH || config->mode > 3U ||
		config->blocking > 3U || config->hysteresis > 15U || config->theta > 63U)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	status = bmi160_update_register(device, BMI160_REG_INT_ENABLE_0, 0x40U,
		(pin == BMI160_INTERRUPT_NONE) ? 0U : 0x40U);
	if (status != BMI160_OK)
	{
		return status;
	}
	data[0] = (uint8_t)(config->mode | (uint8_t)(config->blocking << 2) |
		(uint8_t)(config->hysteresis << 4));
	data[1] = (uint8_t)(config->theta |
		(uint8_t)((config->upside_down & 0x01U) << 6) |
		(uint8_t)((config->axes_exchange & 0x01U) << 7));
	status = bmi160_write_registers(device, BMI160_REG_INT_ORIENT_0, data, sizeof data);
	if (status != BMI160_OK)
	{
		return status;
	}
	return bmi160_route_interrupt(device, BMI160_INTERRUPT_ORIENTATION, pin);
}

bmi160_status_t bmi160_configure_flat(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_flat_config_t *config)
{
	bmi160_status_t status;
	uint8_t data[2];

	if (config == NULL || pin > BMI160_INTERRUPT_BOTH || config->theta > 63U ||
		config->hysteresis > 7U || config->hold_time > 3U)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	status = bmi160_update_register(device, BMI160_REG_INT_ENABLE_0, 0x80U,
		(pin == BMI160_INTERRUPT_NONE) ? 0U : 0x80U);
	if (status != BMI160_OK)
	{
		return status;
	}
	data[0] = config->theta;
	data[1] = (uint8_t)(config->hysteresis | (uint8_t)(config->hold_time << 4));
	status = bmi160_write_registers(device, BMI160_REG_INT_FLAT_0, data, sizeof data);
	if (status != BMI160_OK)
	{
		return status;
	}
	return bmi160_route_interrupt(device, BMI160_INTERRUPT_FLAT, pin);
}

bmi160_status_t bmi160_configure_low_g(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_low_g_config_t *config)
{
	bmi160_status_t status;
	uint8_t data[3];

	if (config == NULL || pin > BMI160_INTERRUPT_BOTH || config->hysteresis > 3U)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	status = bmi160_update_register(device, BMI160_REG_INT_ENABLE_1, 0x08U,
		(pin == BMI160_INTERRUPT_NONE) ? 0U : 0x08U);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_update_register(device, BMI160_REG_INT_DATA_0,
		BMI160_INT_LOW_HIGH_SOURCE, (uint8_t)((config->unfiltered & 0x01U) << 7));
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_read_registers(device, BMI160_REG_INT_LOWHIGH_2, &data[2], 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	data[0] = config->duration;
	data[1] = config->threshold;
	data[2] = (uint8_t)((data[2] & 0xF8U) | config->hysteresis |
		(uint8_t)((config->sum_mode & 0x01U) << 2));
	status = bmi160_write_registers(device, BMI160_REG_INT_LOWHIGH_0, data, sizeof data);
	if (status != BMI160_OK)
	{
		return status;
	}
	return bmi160_route_interrupt(device, BMI160_INTERRUPT_LOW_G, pin);
}

bmi160_status_t bmi160_configure_high_g(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_high_g_config_t *config)
{
	bmi160_status_t status;
	uint8_t axes;
	uint8_t data[3];

	if (config == NULL || pin > BMI160_INTERRUPT_BOTH || config->hysteresis > 3U)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	axes = 0U;
	if (pin != BMI160_INTERRUPT_NONE)
	{
		axes = (uint8_t)((config->x & 0x01U) |
			((config->y & 0x01U) << 1) | ((config->z & 0x01U) << 2));
	}
	status = bmi160_update_register(device, BMI160_REG_INT_ENABLE_1, 0x07U, axes);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_update_register(device, BMI160_REG_INT_DATA_0,
		BMI160_INT_LOW_HIGH_SOURCE, (uint8_t)((config->unfiltered & 0x01U) << 7));
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_read_registers(device, BMI160_REG_INT_LOWHIGH_2, &data[0], 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	data[0] = (uint8_t)((data[0] & 0x3FU) | (uint8_t)(config->hysteresis << 6));
	data[1] = config->duration;
	data[2] = config->threshold;
	status = bmi160_write_registers(device, BMI160_REG_INT_LOWHIGH_2, data, sizeof data);
	if (status != BMI160_OK)
	{
		return status;
	}
	return bmi160_route_interrupt(device, BMI160_INTERRUPT_HIGH_G, pin);
}

bmi160_status_t bmi160_configure_data_ready(const bmi160_t *device, bmi160_interrupt_pin_t pin, uint8_t enable)
{
	bmi160_status_t status;

	if (pin > BMI160_INTERRUPT_BOTH)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	status = bmi160_update_register(device, BMI160_REG_INT_ENABLE_1, 0x10U,
		(enable == 0U) ? 0U : 0x10U);
	if (status != BMI160_OK)
	{
		return status;
	}
	return bmi160_route_interrupt(device, BMI160_INTERRUPT_DATA_READY,
		(enable == 0U) ? BMI160_INTERRUPT_NONE : pin);
}

bmi160_status_t bmi160_configure_fifo_interrupt(const bmi160_t *device,
		bmi160_interrupt_t type, bmi160_interrupt_pin_t pin, uint8_t enable)
{
	bmi160_status_t status;
	uint8_t mask;

	if (pin > BMI160_INTERRUPT_BOTH ||
		(type != BMI160_INTERRUPT_FIFO_FULL && type != BMI160_INTERRUPT_FIFO_WATERMARK))
	{
		return BMI160_ERROR_ARGUMENT;
	}

	mask = (type == BMI160_INTERRUPT_FIFO_FULL) ? 0x20U : 0x40U;
	status = bmi160_update_register(device, BMI160_REG_INT_ENABLE_1, mask,
		(enable == 0U) ? 0U : mask);
	if (status != BMI160_OK)
	{
		return status;
	}
	return bmi160_route_interrupt(device, type,
		(enable == 0U) ? BMI160_INTERRUPT_NONE : pin);
}

bmi160_status_t bmi160_configure_step_detector(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, bmi160_step_mode_t mode, uint8_t enable)
{
	bmi160_status_t status;
	uint8_t config[2];
	uint8_t counter_enable;

	if (pin > BMI160_INTERRUPT_BOTH || mode > BMI160_STEP_ROBUST)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	status = bmi160_read_registers(device, BMI160_REG_STEP_CONFIG_1, &counter_enable, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	if (mode == BMI160_STEP_SENSITIVE)
	{
		config[0] = 0x2DU;
		config[1] = 0x00U;
	}
	else if (mode == BMI160_STEP_ROBUST)
	{
		config[0] = 0x1DU;
		config[1] = 0x07U;
	}
	else
	{
		config[0] = 0x15U;
		config[1] = 0x03U;
	}
	config[1] |= (uint8_t)(counter_enable & 0x08U);
	status = bmi160_write_registers(device, BMI160_REG_STEP_CONFIG_0, config, sizeof config);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_update_register(device, BMI160_REG_INT_ENABLE_2, 0x08U,
		(enable == 0U) ? 0U : 0x08U);
	if (status != BMI160_OK)
	{
		return status;
	}
	return bmi160_route_interrupt(device, BMI160_INTERRUPT_STEP,
		(enable == 0U) ? BMI160_INTERRUPT_NONE : pin);
}

bmi160_status_t bmi160_enable_step_counter(const bmi160_t *device, uint8_t enable)
{
	return bmi160_update_register(device, BMI160_REG_STEP_CONFIG_1, 0x08U,
		(enable == 0U) ? 0U : 0x08U);
}

bmi160_status_t bmi160_read_step_counter(const bmi160_t *device, uint16_t *steps)
{
	bmi160_status_t status;
	uint8_t data[2];

	if (steps == NULL)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	status = bmi160_read_registers(device, BMI160_REG_STEP_COUNT, data, sizeof data);
	if (status == BMI160_OK)
	{
		*steps = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
	}
	return status;
}

bmi160_status_t bmi160_reset_step_counter(const bmi160_t *device)
{
	uint8_t command = BMI160_COMMAND_STEP_RESET;
	return bmi160_write_registers(device, BMI160_REG_COMMAND, &command, 1U);
}

bmi160_status_t bmi160_configure_fifo(bmi160_t *device, const bmi160_fifo_config_t *config)
{
	bmi160_status_t status;
	uint8_t fifo_down;
	uint8_t fifo_config;
	uint8_t watermark;

	if (device == NULL || config == NULL ||
		(config->sensors != BMI160_FIFO_NONE && config->sensors != BMI160_FIFO_ACCEL &&
		 config->sensors != BMI160_FIFO_GYRO && config->sensors != BMI160_FIFO_ACCEL_GYRO) ||
		config->accel_downsample > 7U || config->gyro_downsample > 7U || config->watermark > 1020U ||
		(config->sensor_time != 0U && config->header == 0U))
	{
		return BMI160_ERROR_ARGUMENT;
	}

	fifo_down = (uint8_t)((config->accel_downsample << 4) | config->gyro_downsample);
	if (config->filtered != 0U)
	{
		if (((uint8_t)config->sensors & (uint8_t)BMI160_FIFO_ACCEL) != 0U)
		{
			fifo_down |= 0x80U;
		}
		if (((uint8_t)config->sensors & (uint8_t)BMI160_FIFO_GYRO) != 0U)
		{
			fifo_down |= 0x08U;
		}
	}
	fifo_config = (uint8_t)config->sensors |
		(uint8_t)((config->header & 0x01U) << 4) |
		(uint8_t)((config->int1_tag & 0x01U) << 3) |
		(uint8_t)((config->int2_tag & 0x01U) << 2) |
		(uint8_t)((config->sensor_time & 0x01U) << 1);
	watermark = (uint8_t)((config->watermark + 3U) / 4U);

	status = bmi160_write_registers(device, BMI160_REG_FIFO_DOWN, &fifo_down, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_write_registers(device, BMI160_REG_FIFO_CONFIG_0, &watermark, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_write_registers(device, BMI160_REG_FIFO_CONFIG_1, &fifo_config, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_flush_fifo(device);
	if (status == BMI160_OK)
	{
		device->fifo_sensors = (uint8_t)config->sensors;
		device->fifo_header = config->header;
		device->fifo_sensor_time = config->sensor_time;
	}
	return status;
}

bmi160_status_t bmi160_get_fifo_length(const bmi160_t *device, uint16_t *length)
{
	bmi160_status_t status;
	uint8_t data[2];

	if (length == NULL)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	status = bmi160_read_registers(device, BMI160_REG_FIFO_LENGTH, data, sizeof data);
	if (status == BMI160_OK)
	{
		*length = (uint16_t)data[0] | (uint16_t)(((uint16_t)data[1] & 0x07U) << 8);
	}
	return status;
}

bmi160_status_t bmi160_read_fifo(const bmi160_t *device, uint8_t *data, uint16_t capacity, uint16_t *length)
{
	bmi160_status_t status;
	uint16_t available;
	uint16_t read_length;

	if (data == NULL || capacity == 0U || length == NULL)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	status = bmi160_get_fifo_length(device, &available);
	if (status != BMI160_OK)
	{
		return status;
	}
	*length = available;
	if (available == 0U)
	{
		return BMI160_OK;
	}
	if (available > capacity)
	{
		return BMI160_ERROR_BUFFER;
	}
	read_length = available;
	if (device->fifo_sensor_time != 0U &&
		(uint32_t)available + BMI160_FIFO_OVERREAD <= capacity)
	{
		read_length = (uint16_t)(available + BMI160_FIFO_OVERREAD);
	}
	*length = read_length;

	return bmi160_read_registers(device, BMI160_REG_FIFO_DATA, data, read_length);
}

bmi160_status_t bmi160_flush_fifo(const bmi160_t *device)
{
	uint8_t command = BMI160_COMMAND_FIFO_FLUSH;
	return bmi160_write_registers(device, BMI160_REG_COMMAND, &command, 1U);
}

void bmi160_fifo_parser_init(bmi160_fifo_parser_t *parser,
		const uint8_t *data, uint16_t length, const bmi160_fifo_config_t *config)
{
	if (parser == NULL || config == NULL)
	{
		return;
	}

	parser->data = data;
	parser->length = length;
	parser->index = 0U;
	parser->config = *config;
}

static bmi160_status_t bmi160_fifo_take(bmi160_fifo_parser_t *parser, uint16_t length, const uint8_t **data)
{
	if (parser->data == NULL || parser->index > parser->length ||
		length > (uint16_t)(parser->length - parser->index))
	{
		return BMI160_ERROR_FIFO;
	}

	*data = &parser->data[parser->index];
	parser->index = (uint16_t)(parser->index + length);
	return BMI160_OK;
}

static bmi160_status_t bmi160_fifo_parse_data(bmi160_fifo_parser_t *parser,
		bmi160_fifo_frame_t *frame, bmi160_fifo_frame_type_t type)
{
	bmi160_status_t status;
	const uint8_t *data;
	uint16_t length;

	length = (type == BMI160_FIFO_FRAME_ACCEL_GYRO) ? 12U : 6U;
	status = bmi160_fifo_take(parser, length, &data);
	if (status != BMI160_OK)
	{
		return status;
	}

	frame->type = type;
	if (type == BMI160_FIFO_FRAME_GYRO)
	{
		bmi160_parse_vector(data, &frame->gyro);
	}
	else if (type == BMI160_FIFO_FRAME_ACCEL)
	{
		bmi160_parse_vector(data, &frame->accel);
	}
	else
	{
		bmi160_parse_vector(data, &frame->gyro);
		bmi160_parse_vector(&data[6], &frame->accel);
	}
	return BMI160_OK;
}

bmi160_status_t bmi160_fifo_next(bmi160_fifo_parser_t *parser, bmi160_fifo_frame_t *frame)
{
	bmi160_status_t status;
	bmi160_fifo_frame_type_t type;
	const uint8_t *data;
	uint8_t header;

	if (parser == NULL || frame == NULL || parser->data == NULL)
	{
		return BMI160_ERROR_ARGUMENT;
	}
	memset(frame, 0, sizeof *frame);
	if (parser->index >= parser->length)
	{
		frame->type = BMI160_FIFO_FRAME_END;
		return BMI160_OK;
	}

	if (parser->config.header == 0U)
	{
		if (parser->config.sensor_time != 0U &&
			(uint16_t)(parser->length - parser->index) == 3U)
		{
			status = bmi160_fifo_take(parser, 3U, &data);
			if (status != BMI160_OK)
			{
				return status;
			}
			frame->type = BMI160_FIFO_FRAME_SENSOR_TIME;
			frame->sensor_time = (uint32_t)data[0] |
				((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16);
			return BMI160_OK;
		}

		if (parser->config.sensors == BMI160_FIFO_ACCEL_GYRO)
		{
			type = BMI160_FIFO_FRAME_ACCEL_GYRO;
		}
		else if (parser->config.sensors == BMI160_FIFO_ACCEL)
		{
			type = BMI160_FIFO_FRAME_ACCEL;
		}
		else if (parser->config.sensors == BMI160_FIFO_GYRO)
		{
			type = BMI160_FIFO_FRAME_GYRO;
		}
		else
		{
			return BMI160_ERROR_FIFO;
		}
		return bmi160_fifo_parse_data(parser, frame, type);
	}

	status = bmi160_fifo_take(parser, 1U, &data);
	if (status != BMI160_OK)
	{
		return status;
	}
	header = data[0];
	frame->interrupt_tag = (uint8_t)(header & 0x03U);
	switch (header & 0xFCU)
	{
		case 0x84U:
			return bmi160_fifo_parse_data(parser, frame, BMI160_FIFO_FRAME_ACCEL);
		case 0x88U:
			return bmi160_fifo_parse_data(parser, frame, BMI160_FIFO_FRAME_GYRO);
		case 0x8CU:
			return bmi160_fifo_parse_data(parser, frame, BMI160_FIFO_FRAME_ACCEL_GYRO);
		case 0x44U:
			status = bmi160_fifo_take(parser, 3U, &data);
			if (status == BMI160_OK)
			{
				frame->type = BMI160_FIFO_FRAME_SENSOR_TIME;
				frame->sensor_time = (uint32_t)data[0] |
					((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16);
			}
			return status;
		case 0x40U:
			status = bmi160_fifo_take(parser, 1U, &data);
			if (status == BMI160_OK)
			{
				frame->type = BMI160_FIFO_FRAME_SKIP;
				frame->skipped_frames = data[0];
			}
			return status;
		case 0x48U:
			status = bmi160_fifo_take(parser, 1U, &data);
			if (status == BMI160_OK)
			{
				frame->type = BMI160_FIFO_FRAME_CONFIG;
				frame->config_change = data[0];
			}
			return status;
		case 0x80U:
			frame->type = BMI160_FIFO_FRAME_END;
			parser->index = parser->length;
			return BMI160_OK;
		default:
			return BMI160_ERROR_FIFO;
	}
}

static bmi160_status_t bmi160_accel_self_test(bmi160_t *device, uint8_t *passed)
{
	bmi160_status_t status;
	bmi160_config_t config = device->config;
	bmi160_sample_t positive;
	bmi160_sample_t negative;
	uint8_t self_test;
	int32_t difference_x;
	int32_t difference_y;
	int32_t difference_z;

	config.accel_odr = BMI160_ACCEL_ODR_1600HZ;
	config.accel_bandwidth = BMI160_ACCEL_BW_NORMAL;
	config.accel_range = BMI160_ACCEL_RANGE_8G;
	status = bmi160_apply_config(device, &config);
	if (status != BMI160_OK)
	{
		return status;
	}

	self_test = 0x0DU;
	status = bmi160_write_registers(device, BMI160_REG_SELF_TEST, &self_test, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	delay_ms(50U);
	status = bmi160_read_sample(device, &positive);
	if (status != BMI160_OK)
	{
		return status;
	}

	self_test = 0x09U;
	status = bmi160_write_registers(device, BMI160_REG_SELF_TEST, &self_test, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	delay_ms(50U);
	status = bmi160_read_sample(device, &negative);
	if (status != BMI160_OK)
	{
		return status;
	}

	difference_x = (int32_t)positive.accel.x - (int32_t)negative.accel.x;
	difference_y = (int32_t)positive.accel.y - (int32_t)negative.accel.y;
	difference_z = (int32_t)positive.accel.z - (int32_t)negative.accel.z;
	if (difference_x < 0)
	{
		difference_x = -difference_x;
	}
	if (difference_y < 0)
	{
		difference_y = -difference_y;
	}
	if (difference_z < 0)
	{
		difference_z = -difference_z;
	}
	*passed = (difference_x > 8192L && difference_y > 8192L && difference_z > 8192L) ? 1U : 0U;
	return BMI160_OK;
}

static bmi160_status_t bmi160_gyro_self_test(bmi160_t *device, uint8_t *passed)
{
	bmi160_status_t status;
	uint8_t self_test;
	uint8_t sensor_status;

	status = bmi160_set_gyro_power(device, BMI160_GYRO_NORMAL);
	if (status != BMI160_OK)
	{
		return status;
	}
	status = bmi160_read_registers(device, BMI160_REG_SELF_TEST, &self_test, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	self_test |= 0x10U;
	status = bmi160_write_registers(device, BMI160_REG_SELF_TEST, &self_test, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	delay_ms(50U);
	status = bmi160_read_registers(device, BMI160_REG_STATUS, &sensor_status, 1U);
	if (status == BMI160_OK)
	{
		*passed = ((sensor_status & BMI160_STATUS_GYRO_SELF_TEST) != 0U) ? 1U : 0U;
	}
	return status;
}

static bmi160_status_t bmi160_restore_after_test(bmi160_t *device, const bmi160_config_t *config)
{
	bmi160_status_t status;

	status = bmi160_soft_reset(device);
	if (status != BMI160_OK)
	{
		return status;
	}
	return bmi160_apply_config(device, config);
}

bmi160_status_t bmi160_run_self_test(bmi160_t *device, bmi160_self_test_result_t *result)
{
	bmi160_status_t status;
	bmi160_status_t restore_status;
	bmi160_config_t config;

	if (device == NULL || result == NULL)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	config = device->config;
	result->accel_passed = 0U;
	result->gyro_passed = 0U;
	status = bmi160_accel_self_test(device, &result->accel_passed);
	restore_status = bmi160_restore_after_test(device, &config);
	if (restore_status != BMI160_OK)
	{
		return restore_status;
	}
	if (status != BMI160_OK)
	{
		return status;
	}

	status = bmi160_gyro_self_test(device, &result->gyro_passed);
	restore_status = bmi160_restore_after_test(device, &config);
	if (restore_status != BMI160_OK)
	{
		return restore_status;
	}
	if (status != BMI160_OK)
	{
		return status;
	}
	if (result->accel_passed == 0U || result->gyro_passed == 0U)
	{
		return BMI160_ERROR_SELF_TEST;
	}

	return BMI160_OK;
}

static int16_t bmi160_sign_extend_10(uint16_t value)
{
	if ((value & 0x0200U) != 0U)
	{
		value |= 0xFC00U;
	}
	return (int16_t)value;
}

bmi160_status_t bmi160_get_offsets(const bmi160_t *device, bmi160_offsets_t *offsets)
{
	bmi160_status_t status;
	uint8_t data[7];

	if (offsets == NULL)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	status = bmi160_read_registers(device, BMI160_REG_OFFSET_0, data, sizeof data);
	if (status != BMI160_OK)
	{
		return status;
	}
	offsets->accel_x = (int8_t)data[0];
	offsets->accel_y = (int8_t)data[1];
	offsets->accel_z = (int8_t)data[2];
	offsets->gyro_x = bmi160_sign_extend_10((uint16_t)data[3] | (uint16_t)((data[6] & 0x03U) << 8));
	offsets->gyro_y = bmi160_sign_extend_10((uint16_t)data[4] | (uint16_t)((data[6] & 0x0CU) << 6));
	offsets->gyro_z = bmi160_sign_extend_10((uint16_t)data[5] | (uint16_t)((data[6] & 0x30U) << 4));
	return BMI160_OK;
}

bmi160_status_t bmi160_set_offsets(const bmi160_t *device,
		const bmi160_offsets_t *offsets, uint8_t accel_enable, uint8_t gyro_enable)
{
	uint8_t data[7];
	uint16_t gyro_x;
	uint16_t gyro_y;
	uint16_t gyro_z;

	if (device == NULL || offsets == NULL || offsets->gyro_x < -512 || offsets->gyro_x > 511 ||
		offsets->gyro_y < -512 || offsets->gyro_y > 511 || offsets->gyro_z < -512 || offsets->gyro_z > 511)
	{
		return BMI160_ERROR_ARGUMENT;
	}

	gyro_x = (uint16_t)offsets->gyro_x & 0x03FFU;
	gyro_y = (uint16_t)offsets->gyro_y & 0x03FFU;
	gyro_z = (uint16_t)offsets->gyro_z & 0x03FFU;
	data[0] = (uint8_t)offsets->accel_x;
	data[1] = (uint8_t)offsets->accel_y;
	data[2] = (uint8_t)offsets->accel_z;
	data[3] = (uint8_t)gyro_x;
	data[4] = (uint8_t)gyro_y;
	data[5] = (uint8_t)gyro_z;
	data[6] = (uint8_t)((gyro_x >> 8) | ((gyro_y >> 8) << 2) | ((gyro_z >> 8) << 4));
	data[6] |= (uint8_t)((accel_enable & 0x01U) << 6) |
		(uint8_t)((gyro_enable & 0x01U) << 7);
	return bmi160_write_registers(device, BMI160_REG_OFFSET_0, data, sizeof data);
}

bmi160_status_t bmi160_start_foc(const bmi160_t *device, const bmi160_foc_config_t *config)
{
	bmi160_status_t status;
	bmi160_power_status_t power;
	uint8_t foc_config;
	uint8_t offset_config;
	uint8_t command = BMI160_COMMAND_START_FOC;

	if (device == NULL || config == NULL ||
		config->accel_x > BMI160_FOC_ZERO_G || config->accel_y > BMI160_FOC_ZERO_G ||
		config->accel_z > BMI160_FOC_ZERO_G || config->gyro > 1U)
	{
		return BMI160_ERROR_ARGUMENT;
	}
	status = bmi160_get_power_status(device, &power);
	if (status != BMI160_OK)
	{
		return status;
	}
	if (((config->accel_x != BMI160_FOC_DISABLED || config->accel_y != BMI160_FOC_DISABLED ||
		  config->accel_z != BMI160_FOC_DISABLED) && power.accel != 0x01U) ||
		(config->gyro != 0U && power.gyro != 0x01U))
	{
		return BMI160_ERROR_NOT_READY;
	}

	status = bmi160_read_registers(device, BMI160_REG_OFFSET_6, &offset_config, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	offset_config = (uint8_t)((offset_config & 0x3FU) |
		(uint8_t)((config->enable_accel_offset & 0x01U) << 6) |
		(uint8_t)((config->enable_gyro_offset & 0x01U) << 7));
	status = bmi160_write_registers(device, BMI160_REG_OFFSET_6, &offset_config, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}

	foc_config = (uint8_t)((config->accel_z & 0x03U) |
		(uint8_t)((config->accel_y & 0x03U) << 2) |
		(uint8_t)((config->accel_x & 0x03U) << 4) |
		(uint8_t)((config->gyro & 0x01U) << 6));
	status = bmi160_write_registers(device, BMI160_REG_FOC_CONFIG, &foc_config, 1U);
	if (status != BMI160_OK)
	{
		return status;
	}
	return bmi160_write_registers(device, BMI160_REG_COMMAND, &command, 1U);
}

bmi160_status_t bmi160_foc_ready(const bmi160_t *device, uint8_t *ready)
{
	bmi160_status_t status;
	uint8_t sensor_status;

	if (ready == NULL)
	{
		return BMI160_ERROR_ARGUMENT;
	}
	status = bmi160_read_registers(device, BMI160_REG_STATUS, &sensor_status, 1U);
	if (status == BMI160_OK)
	{
		*ready = ((sensor_status & BMI160_STATUS_FOC_DONE) != 0U) ? 1U : 0U;
	}
	return status;
}

bmi160_status_t bmi160_run_foc(const bmi160_t *device,
		const bmi160_foc_config_t *config, bmi160_offsets_t *offsets)
{
	bmi160_status_t status;
	uint8_t ready;
	uint8_t attempt;

	if (offsets == NULL)
	{
		return BMI160_ERROR_ARGUMENT;
	}
	status = bmi160_start_foc(device, config);
	if (status != BMI160_OK)
	{
		return status;
	}

	for (attempt = 0U; attempt < 11U; attempt++)
	{
		status = bmi160_foc_ready(device, &ready);
		if (status != BMI160_OK)
		{
			return status;
		}
		if (ready != 0U)
		{
			return bmi160_get_offsets(device, offsets);
		}
		if (attempt < 10U)
		{
			delay_ms(25U);
		}
	}

	return BMI160_ERROR_TIMEOUT;
}
