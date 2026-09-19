#include "stm32f10x.h"
#include "board.h"
#include "bsp_bmi160.h"
#include "bsp_i2c.h"
#include "bsp_uart.h"
#include <stdio.h>

#define CALIBRATION_SAMPLE_COUNT 50U
#define CALIBRATION_SAMPLE_DELAY 20U
#define CALIBRATION_GRAVITY_MIN  750L
#define CALIBRATION_GRAVITY_MAX  1250L
#define CALIBRATION_CROSS_MAX    400L

static int32_t absolute_value(int32_t value)
{
	return (value < 0) ? -value : value;
}

static void stop_with_error(bmi160_status_t status)
{
	printf("BMI160 error: %d\r\n", (int)status);
	while (1)
	{
		board_led_on();
		delay_ms(100U);
		board_led_off();
		delay_ms(100U);
	}
}

static bmi160_status_t calibrate_bmi160(bmi160_t *device)
{
	bmi160_foc_config_t config;
	bmi160_offsets_t offsets;
	bmi160_sample_t sample;
	bmi160_scaled_sample_t scaled;
	bmi160_status_t status;
	int32_t accel_x = 0L;
	int32_t accel_y = 0L;
	int32_t accel_z = 0L;
	int32_t absolute_x;
	int32_t absolute_y;
	int32_t absolute_z;
	char axis;
	char sign;
	uint8_t ready;
	uint8_t index;

	config.accel_x = BMI160_FOC_ZERO_G;
	config.accel_y = BMI160_FOC_ZERO_G;
	config.accel_z = BMI160_FOC_POSITIVE_G;
	config.gyro = 1U;
	config.enable_accel_offset = 1U;
	config.enable_gyro_offset = 1U;

	printf("BMI160 calibration: keep still on a flat face\r\n");
	for (index = 0U; index < CALIBRATION_SAMPLE_COUNT; index++)
	{
		status = bmi160_read_sample(device, &sample);
		if (status != BMI160_OK)
		{
			return status;
		}
		bmi160_scale_sample(device, &sample, &scaled);
		accel_x += scaled.accel_mg.x;
		accel_y += scaled.accel_mg.y;
		accel_z += scaled.accel_mg.z;
		if ((index % 5U) == 0U)
		{
			board_led_toggle();
		}
		delay_ms(CALIBRATION_SAMPLE_DELAY);
	}
	accel_x /= (int32_t)CALIBRATION_SAMPLE_COUNT;
	accel_y /= (int32_t)CALIBRATION_SAMPLE_COUNT;
	accel_z /= (int32_t)CALIBRATION_SAMPLE_COUNT;
	absolute_x = absolute_value(accel_x);
	absolute_y = absolute_value(accel_y);
	absolute_z = absolute_value(accel_z);

	if (absolute_x >= absolute_y && absolute_x >= absolute_z)
	{
		if (absolute_x < CALIBRATION_GRAVITY_MIN || absolute_x > CALIBRATION_GRAVITY_MAX ||
			absolute_y > CALIBRATION_CROSS_MAX || absolute_z > CALIBRATION_CROSS_MAX)
		{
			printf("BMI160 calibration pose invalid: A[mg] %ld %ld %ld\r\n",
				(long)accel_x, (long)accel_y, (long)accel_z);
			return BMI160_ERROR_NOT_READY;
		}
		config.accel_x = (accel_x < 0L) ? BMI160_FOC_NEGATIVE_G : BMI160_FOC_POSITIVE_G;
		axis = 'X';
		sign = (accel_x < 0L) ? '-' : '+';
	}
	else if (absolute_y >= absolute_z)
	{
		if (absolute_y < CALIBRATION_GRAVITY_MIN || absolute_y > CALIBRATION_GRAVITY_MAX ||
			absolute_x > CALIBRATION_CROSS_MAX || absolute_z > CALIBRATION_CROSS_MAX)
		{
			printf("BMI160 calibration pose invalid: A[mg] %ld %ld %ld\r\n",
				(long)accel_x, (long)accel_y, (long)accel_z);
			return BMI160_ERROR_NOT_READY;
		}
		config.accel_y = (accel_y < 0L) ? BMI160_FOC_NEGATIVE_G : BMI160_FOC_POSITIVE_G;
		axis = 'Y';
		sign = (accel_y < 0L) ? '-' : '+';
	}
	else
	{
		if (absolute_z < CALIBRATION_GRAVITY_MIN || absolute_z > CALIBRATION_GRAVITY_MAX ||
			absolute_x > CALIBRATION_CROSS_MAX || absolute_y > CALIBRATION_CROSS_MAX)
		{
			printf("BMI160 calibration pose invalid: A[mg] %ld %ld %ld\r\n",
				(long)accel_x, (long)accel_y, (long)accel_z);
			return BMI160_ERROR_NOT_READY;
		}
		config.accel_z = (accel_z < 0L) ? BMI160_FOC_NEGATIVE_G : BMI160_FOC_POSITIVE_G;
		axis = 'Z';
		sign = (accel_z < 0L) ? '-' : '+';
	}
	printf("BMI160 gravity axis: %c%c\r\n", axis, sign);

	status = bmi160_start_foc(device, &config);
	if (status != BMI160_OK)
	{
		return status;
	}
	for (index = 0U; index < 6U; index++)
	{
		status = bmi160_foc_ready(device, &ready);
		if (status != BMI160_OK)
		{
			return status;
		}
		if (ready != 0U)
		{
			status = bmi160_get_offsets(device, &offsets);
			if (status == BMI160_OK)
			{
				printf("BMI160 calibrated: A %d %d %d, G %d %d %d\r\n",
					(int)offsets.accel_x, (int)offsets.accel_y, (int)offsets.accel_z,
					(int)offsets.gyro_x, (int)offsets.gyro_y, (int)offsets.gyro_z);
			}
			return status;
		}
		if (index < 5U)
		{
			board_led_toggle();
			delay_ms(50U);
		}
	}

	return BMI160_ERROR_TIMEOUT;
}

int main(void)
{
	bmi160_t bmi160;
	bmi160_config_t config;
	bmi160_sample_t sample;
	bmi160_scaled_sample_t scaled;
	bmi160_status_t status;
	int32_t temperature;

	board_init();
	board_led_init();
	uart1_init(115200U);
	i2c1_init();

	config = bmi160_default_config();
	status = bmi160_init(&bmi160, BMI160_I2C_ADDRESS_LOW, &config);
	if (status != BMI160_OK)
	{
		stop_with_error(status);
	}
	printf("BMI160 ready, chip ID: 0x%02X\r\n", (unsigned)bmi160.chip_id);
	status = calibrate_bmi160(&bmi160);
	if (status != BMI160_OK)
	{
		stop_with_error(status);
	}
	board_led_on();

	while (1)
	{
		status = bmi160_read_sample(&bmi160, &sample);
		if (status != BMI160_OK)
		{
			stop_with_error(status);
		}
		status = bmi160_read_temperature(&bmi160, &temperature);
		if (status != BMI160_OK)
		{
			stop_with_error(status);
		}

		bmi160_scale_sample(&bmi160, &sample, &scaled);
		printf("A[mg] %ld %ld %ld  G[mdps] %ld %ld %ld  T[mC] %ld  time %lu\r\n",
			(long)scaled.accel_mg.x, (long)scaled.accel_mg.y, (long)scaled.accel_mg.z,
			(long)scaled.gyro_mdps.x, (long)scaled.gyro_mdps.y, (long)scaled.gyro_mdps.z,
			(long)temperature, (unsigned long)scaled.sensor_time);

		delay_ms(100U);
	}
}
