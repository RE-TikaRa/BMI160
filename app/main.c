#include "stm32f10x.h"
#include "board.h"
#include "bsp_bmi160.h"
#include "bsp_i2c.h"
#include "bsp_uart.h"
#include <math.h>
#include <stdio.h>

#define CALIBRATION_SAMPLE_COUNT 50U
#define CALIBRATION_SAMPLE_DELAY 20U
#define CALIBRATION_STABLE_COUNT 25U
#define CALIBRATION_WAIT_LIMIT   500U
#define CALIBRATION_GRAVITY_MIN  750L
#define CALIBRATION_GRAVITY_MAX  1250L
#define CALIBRATION_ACCEL_DELTA  25L
#define CALIBRATION_GYRO_MAX     5000L

typedef struct
{
	float rotation[3][3];
	bmi160_scaled_vector_t gyro_bias_mdps;
} startup_frame_t;

static int32_t absolute_value(int32_t value)
{
	return (value < 0) ? -value : value;
}

static int32_t round_float(float value)
{
	return (int32_t)(value + ((value < 0.0f) ? -0.5f : 0.5f));
}

static uint8_t calibration_sample_is_stable(const bmi160_scaled_sample_t *sample,
		const bmi160_scaled_sample_t *previous)
{
	if (absolute_value(sample->gyro_mdps.x) > CALIBRATION_GYRO_MAX ||
		absolute_value(sample->gyro_mdps.y) > CALIBRATION_GYRO_MAX ||
		absolute_value(sample->gyro_mdps.z) > CALIBRATION_GYRO_MAX)
	{
		return 0U;
	}
	if (absolute_value(sample->accel_mg.x - previous->accel_mg.x) > CALIBRATION_ACCEL_DELTA ||
		absolute_value(sample->accel_mg.y - previous->accel_mg.y) > CALIBRATION_ACCEL_DELTA ||
		absolute_value(sample->accel_mg.z - previous->accel_mg.z) > CALIBRATION_ACCEL_DELTA)
	{
		return 0U;
	}

	return 1U;
}

static uint8_t startup_frame_init(startup_frame_t *frame, int32_t accel_x, int32_t accel_y, int32_t accel_z)
{
	float gravity[3];
	float reference[3];
	float horizontal_x[3];
	float horizontal_y[3];
	float gravity_length;
	float projection;
	float horizontal_length;
	uint8_t index;

	gravity_length = sqrtf((float)accel_x * (float)accel_x +
		(float)accel_y * (float)accel_y + (float)accel_z * (float)accel_z);
	if (gravity_length < (float)CALIBRATION_GRAVITY_MIN ||
		gravity_length > (float)CALIBRATION_GRAVITY_MAX)
	{
		return 0U;
	}
	gravity[0] = (float)accel_x / gravity_length;
	gravity[1] = (float)accel_y / gravity_length;
	gravity[2] = (float)accel_z / gravity_length;

	if (gravity[0] > -0.9f && gravity[0] < 0.9f)
	{
		reference[0] = 1.0f;
		reference[1] = 0.0f;
		reference[2] = 0.0f;
	}
	else
	{
		reference[0] = 0.0f;
		reference[1] = 1.0f;
		reference[2] = 0.0f;
	}
	projection = reference[0] * gravity[0] +
		reference[1] * gravity[1] + reference[2] * gravity[2];
	for (index = 0U; index < 3U; index++)
	{
		horizontal_x[index] = reference[index] - projection * gravity[index];
	}
	horizontal_length = sqrtf(horizontal_x[0] * horizontal_x[0] +
		horizontal_x[1] * horizontal_x[1] + horizontal_x[2] * horizontal_x[2]);
	for (index = 0U; index < 3U; index++)
	{
		horizontal_x[index] /= horizontal_length;
	}
	horizontal_y[0] = gravity[1] * horizontal_x[2] - gravity[2] * horizontal_x[1];
	horizontal_y[1] = gravity[2] * horizontal_x[0] - gravity[0] * horizontal_x[2];
	horizontal_y[2] = gravity[0] * horizontal_x[1] - gravity[1] * horizontal_x[0];

	for (index = 0U; index < 3U; index++)
	{
		frame->rotation[0][index] = horizontal_x[index];
		frame->rotation[1][index] = horizontal_y[index];
		frame->rotation[2][index] = gravity[index];
	}
	return 1U;
}

static void startup_frame_rotate_vector(const startup_frame_t *frame, bmi160_scaled_vector_t *vector)
{
	int32_t source_x = vector->x;
	int32_t source_y = vector->y;
	int32_t source_z = vector->z;
	uint8_t row;
	int32_t result[3];

	for (row = 0U; row < 3U; row++)
	{
		result[row] = round_float(frame->rotation[row][0] * (float)source_x +
			frame->rotation[row][1] * (float)source_y +
			frame->rotation[row][2] * (float)source_z);
	}
	vector->x = result[0];
	vector->y = result[1];
	vector->z = result[2];
}

static void startup_frame_rotate_sample(const startup_frame_t *frame, bmi160_scaled_sample_t *sample)
{
	sample->gyro_mdps.x -= frame->gyro_bias_mdps.x;
	sample->gyro_mdps.y -= frame->gyro_bias_mdps.y;
	sample->gyro_mdps.z -= frame->gyro_bias_mdps.z;
	startup_frame_rotate_vector(frame, &sample->accel_mg);
	startup_frame_rotate_vector(frame, &sample->gyro_mdps);
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

static bmi160_status_t calibrate_bmi160(const bmi160_t *device, startup_frame_t *frame)
{
	bmi160_sample_t sample;
	bmi160_scaled_sample_t scaled;
	bmi160_scaled_sample_t previous;
	bmi160_status_t status;
	int32_t accel_x = 0L;
	int32_t accel_y = 0L;
	int32_t accel_z = 0L;
	int32_t gyro_x = 0L;
	int32_t gyro_y = 0L;
	int32_t gyro_z = 0L;
	uint16_t wait_index = 0U;
	uint8_t stable_samples = 0U;
	uint8_t has_previous = 0U;
	uint8_t index;

	printf("BMI160 calibration: waiting for stillness\r\n");
	while (stable_samples < CALIBRATION_STABLE_COUNT && wait_index < CALIBRATION_WAIT_LIMIT)
	{
		status = bmi160_read_sample(device, &sample);
		if (status != BMI160_OK)
		{
			return status;
		}
		bmi160_scale_sample(device, &sample, &scaled);
		if (has_previous != 0U && calibration_sample_is_stable(&scaled, &previous) != 0U)
		{
			stable_samples++;
		}
		else
		{
			stable_samples = 0U;
		}
		previous = scaled;
		has_previous = 1U;
		if ((wait_index % 5U) == 0U)
		{
			board_led_toggle();
		}
		delay_ms(CALIBRATION_SAMPLE_DELAY);
		wait_index++;
	}
	if (stable_samples < CALIBRATION_STABLE_COUNT)
	{
		printf("BMI160 stillness timeout: A[mg] %ld %ld %ld  G[mdps] %ld %ld %ld\r\n",
			(long)scaled.accel_mg.x, (long)scaled.accel_mg.y, (long)scaled.accel_mg.z,
			(long)scaled.gyro_mdps.x, (long)scaled.gyro_mdps.y, (long)scaled.gyro_mdps.z);
		return BMI160_ERROR_NOT_READY;
	}
	printf("BMI160 calibration: sampling\r\n");
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
		gyro_x += scaled.gyro_mdps.x;
		gyro_y += scaled.gyro_mdps.y;
		gyro_z += scaled.gyro_mdps.z;
		if ((index % 5U) == 0U)
		{
			board_led_toggle();
		}
		delay_ms(CALIBRATION_SAMPLE_DELAY);
	}
	accel_x /= (int32_t)CALIBRATION_SAMPLE_COUNT;
	accel_y /= (int32_t)CALIBRATION_SAMPLE_COUNT;
	accel_z /= (int32_t)CALIBRATION_SAMPLE_COUNT;
	gyro_x /= (int32_t)CALIBRATION_SAMPLE_COUNT;
	gyro_y /= (int32_t)CALIBRATION_SAMPLE_COUNT;
	gyro_z /= (int32_t)CALIBRATION_SAMPLE_COUNT;

	if (startup_frame_init(frame, accel_x, accel_y, accel_z) == 0U)
	{
		printf("BMI160 gravity invalid: A[mg] %ld %ld %ld\r\n",
			(long)accel_x, (long)accel_y, (long)accel_z);
		return BMI160_ERROR_NOT_READY;
	}
	if (absolute_value(gyro_x) > CALIBRATION_GYRO_MAX ||
		absolute_value(gyro_y) > CALIBRATION_GYRO_MAX ||
		absolute_value(gyro_z) > CALIBRATION_GYRO_MAX)
	{
		printf("BMI160 motion detected: G[mdps] %ld %ld %ld\r\n",
			(long)gyro_x, (long)gyro_y, (long)gyro_z);
		return BMI160_ERROR_NOT_READY;
	}
	printf("BMI160 startup frame: A[mg] %ld %ld %ld\r\n",
		(long)accel_x, (long)accel_y, (long)accel_z);
	frame->gyro_bias_mdps.x = gyro_x;
	frame->gyro_bias_mdps.y = gyro_y;
	frame->gyro_bias_mdps.z = gyro_z;
	printf("BMI160 gyro bias[mdps]: %ld %ld %ld\r\n",
		(long)gyro_x, (long)gyro_y, (long)gyro_z);
	return BMI160_OK;
}

int main(void)
{
	bmi160_t bmi160;
	bmi160_config_t config;
	bmi160_sample_t sample;
	bmi160_scaled_sample_t scaled;
	bmi160_status_t status;
	startup_frame_t frame;
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
	status = calibrate_bmi160(&bmi160, &frame);
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
		startup_frame_rotate_sample(&frame, &scaled);
		printf("A[mg] %ld %ld %ld  G[mdps] %ld %ld %ld  T[mC] %ld  time %lu\r\n",
			(long)scaled.accel_mg.x, (long)scaled.accel_mg.y, (long)scaled.accel_mg.z,
			(long)scaled.gyro_mdps.x, (long)scaled.gyro_mdps.y, (long)scaled.gyro_mdps.z,
			(long)temperature, (unsigned long)scaled.sensor_time);

		delay_ms(100U);
	}
}
