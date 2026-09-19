#ifndef BSP_BMI160_H
#define BSP_BMI160_H

#include "stm32f10x.h"

#define BMI160_I2C_ADDRESS_LOW  0x68U
#define BMI160_I2C_ADDRESS_HIGH 0x69U
#define BMI160_CHIP_ID           0xD1U
#define BMI160_FIFO_SIZE         1024U
#define BMI160_FIFO_OVERREAD     25U

typedef enum
{
	BMI160_OK = 0,
	BMI160_ERROR_ARGUMENT,
	BMI160_ERROR_I2C,
	BMI160_ERROR_TIMEOUT,
	BMI160_ERROR_DEVICE,
	BMI160_ERROR_CONFIG,
	BMI160_ERROR_NOT_READY,
	BMI160_ERROR_BUFFER,
	BMI160_ERROR_FIFO,
	BMI160_ERROR_SELF_TEST
} bmi160_status_t;

typedef enum
{
	BMI160_ACCEL_ODR_0_78HZ = 0x01,
	BMI160_ACCEL_ODR_1_56HZ = 0x02,
	BMI160_ACCEL_ODR_3_12HZ = 0x03,
	BMI160_ACCEL_ODR_6_25HZ = 0x04,
	BMI160_ACCEL_ODR_12_5HZ = 0x05,
	BMI160_ACCEL_ODR_25HZ = 0x06,
	BMI160_ACCEL_ODR_50HZ = 0x07,
	BMI160_ACCEL_ODR_100HZ = 0x08,
	BMI160_ACCEL_ODR_200HZ = 0x09,
	BMI160_ACCEL_ODR_400HZ = 0x0A,
	BMI160_ACCEL_ODR_800HZ = 0x0B,
	BMI160_ACCEL_ODR_1600HZ = 0x0C
} bmi160_accel_odr_t;

typedef enum
{
	BMI160_GYRO_ODR_25HZ = 0x06,
	BMI160_GYRO_ODR_50HZ = 0x07,
	BMI160_GYRO_ODR_100HZ = 0x08,
	BMI160_GYRO_ODR_200HZ = 0x09,
	BMI160_GYRO_ODR_400HZ = 0x0A,
	BMI160_GYRO_ODR_800HZ = 0x0B,
	BMI160_GYRO_ODR_1600HZ = 0x0C,
	BMI160_GYRO_ODR_3200HZ = 0x0D
} bmi160_gyro_odr_t;

typedef enum
{
	BMI160_ACCEL_BW_OSR4 = 0x00,
	BMI160_ACCEL_BW_OSR2 = 0x01,
	BMI160_ACCEL_BW_NORMAL = 0x02,
	BMI160_ACCEL_BW_RES_AVG8 = 0x03,
	BMI160_ACCEL_BW_RES_AVG16 = 0x04,
	BMI160_ACCEL_BW_RES_AVG32 = 0x05,
	BMI160_ACCEL_BW_RES_AVG64 = 0x06,
	BMI160_ACCEL_BW_RES_AVG128 = 0x07
} bmi160_accel_bandwidth_t;

typedef enum
{
	BMI160_GYRO_BW_OSR4 = 0x00,
	BMI160_GYRO_BW_OSR2 = 0x01,
	BMI160_GYRO_BW_NORMAL = 0x02
} bmi160_gyro_bandwidth_t;

typedef enum
{
	BMI160_ACCEL_RANGE_2G = 0x03,
	BMI160_ACCEL_RANGE_4G = 0x05,
	BMI160_ACCEL_RANGE_8G = 0x08,
	BMI160_ACCEL_RANGE_16G = 0x0C
} bmi160_accel_range_t;

typedef enum
{
	BMI160_GYRO_RANGE_2000DPS = 0x00,
	BMI160_GYRO_RANGE_1000DPS = 0x01,
	BMI160_GYRO_RANGE_500DPS = 0x02,
	BMI160_GYRO_RANGE_250DPS = 0x03,
	BMI160_GYRO_RANGE_125DPS = 0x04
} bmi160_gyro_range_t;

typedef enum
{
	BMI160_ACCEL_SUSPEND = 0x10,
	BMI160_ACCEL_NORMAL = 0x11,
	BMI160_ACCEL_LOW_POWER = 0x12
} bmi160_accel_power_t;

typedef enum
{
	BMI160_GYRO_SUSPEND = 0x14,
	BMI160_GYRO_NORMAL = 0x15,
	BMI160_GYRO_FAST_STARTUP = 0x17
} bmi160_gyro_power_t;

typedef enum
{
	BMI160_INTERRUPT_NONE = 0,
	BMI160_INTERRUPT_1 = 1,
	BMI160_INTERRUPT_2 = 2,
	BMI160_INTERRUPT_BOTH = 3
} bmi160_interrupt_pin_t;

typedef enum
{
	BMI160_INTERRUPT_ANY_MOTION,
	BMI160_INTERRUPT_SIGNIFICANT_MOTION,
	BMI160_INTERRUPT_STEP,
	BMI160_INTERRUPT_DOUBLE_TAP,
	BMI160_INTERRUPT_SINGLE_TAP,
	BMI160_INTERRUPT_ORIENTATION,
	BMI160_INTERRUPT_FLAT,
	BMI160_INTERRUPT_HIGH_G,
	BMI160_INTERRUPT_LOW_G,
	BMI160_INTERRUPT_NO_MOTION,
	BMI160_INTERRUPT_DATA_READY,
	BMI160_INTERRUPT_FIFO_FULL,
	BMI160_INTERRUPT_FIFO_WATERMARK
} bmi160_interrupt_t;

typedef enum
{
	BMI160_STEP_NORMAL,
	BMI160_STEP_SENSITIVE,
	BMI160_STEP_ROBUST
} bmi160_step_mode_t;

typedef enum
{
	BMI160_FOC_DISABLED = 0,
	BMI160_FOC_POSITIVE_G = 1,
	BMI160_FOC_NEGATIVE_G = 2,
	BMI160_FOC_ZERO_G = 3
} bmi160_foc_target_t;

typedef enum
{
	BMI160_FIFO_NONE = 0x00,
	BMI160_FIFO_ACCEL = 0x40,
	BMI160_FIFO_GYRO = 0x80,
	BMI160_FIFO_ACCEL_GYRO = 0xC0
} bmi160_fifo_sensors_t;

typedef enum
{
	BMI160_FIFO_FRAME_NONE,
	BMI160_FIFO_FRAME_ACCEL,
	BMI160_FIFO_FRAME_GYRO,
	BMI160_FIFO_FRAME_ACCEL_GYRO,
	BMI160_FIFO_FRAME_SENSOR_TIME,
	BMI160_FIFO_FRAME_SKIP,
	BMI160_FIFO_FRAME_CONFIG,
	BMI160_FIFO_FRAME_END
} bmi160_fifo_frame_type_t;

typedef struct
{
	int16_t x;
	int16_t y;
	int16_t z;
} bmi160_vector_t;

typedef struct
{
	bmi160_vector_t accel;
	bmi160_vector_t gyro;
	uint32_t sensor_time;
} bmi160_sample_t;

typedef struct
{
	int32_t x;
	int32_t y;
	int32_t z;
} bmi160_scaled_vector_t;

typedef struct
{
	bmi160_scaled_vector_t accel_mg;
	bmi160_scaled_vector_t gyro_mdps;
	uint32_t sensor_time;
} bmi160_scaled_sample_t;

typedef struct
{
	bmi160_accel_odr_t accel_odr;
	bmi160_accel_bandwidth_t accel_bandwidth;
	bmi160_accel_range_t accel_range;
	bmi160_gyro_odr_t gyro_odr;
	bmi160_gyro_bandwidth_t gyro_bandwidth;
	bmi160_gyro_range_t gyro_range;
} bmi160_config_t;

typedef struct
{
	uint8_t address;
	uint8_t chip_id;
	uint8_t fifo_sensors;
	uint8_t fifo_header;
	uint8_t fifo_sensor_time;
	bmi160_config_t config;
} bmi160_t;

typedef struct
{
	uint8_t accel;
	uint8_t gyro;
} bmi160_power_status_t;

typedef struct
{
	uint8_t active_high;
	uint8_t open_drain;
	uint8_t edge_triggered;
	uint8_t input_enable;
	uint8_t latch;
} bmi160_interrupt_pin_config_t;

typedef struct
{
	uint8_t x;
	uint8_t y;
	uint8_t z;
	uint8_t duration;
	uint8_t threshold;
	uint8_t unfiltered;
} bmi160_motion_config_t;

typedef struct
{
	uint8_t skip;
	uint8_t proof;
	uint8_t threshold;
	uint8_t unfiltered;
} bmi160_significant_motion_config_t;

typedef struct
{
	uint8_t duration;
	uint8_t shock;
	uint8_t quiet;
	uint8_t threshold;
	uint8_t unfiltered;
} bmi160_tap_config_t;

typedef struct
{
	uint8_t mode;
	uint8_t blocking;
	uint8_t hysteresis;
	uint8_t theta;
	uint8_t upside_down;
	uint8_t axes_exchange;
} bmi160_orientation_config_t;

typedef struct
{
	uint8_t theta;
	uint8_t hysteresis;
	uint8_t hold_time;
} bmi160_flat_config_t;

typedef struct
{
	uint8_t duration;
	uint8_t threshold;
	uint8_t hysteresis;
	uint8_t sum_mode;
	uint8_t unfiltered;
} bmi160_low_g_config_t;

typedef struct
{
	uint8_t x;
	uint8_t y;
	uint8_t z;
	uint8_t duration;
	uint8_t threshold;
	uint8_t hysteresis;
	uint8_t unfiltered;
} bmi160_high_g_config_t;

typedef struct
{
	bmi160_fifo_sensors_t sensors;
	uint8_t header;
	uint8_t sensor_time;
	uint8_t int1_tag;
	uint8_t int2_tag;
	uint8_t accel_downsample;
	uint8_t gyro_downsample;
	uint8_t filtered;
	uint16_t watermark;
} bmi160_fifo_config_t;

typedef struct
{
	const uint8_t *data;
	uint16_t length;
	uint16_t index;
	bmi160_fifo_config_t config;
} bmi160_fifo_parser_t;

typedef struct
{
	bmi160_fifo_frame_type_t type;
	bmi160_vector_t accel;
	bmi160_vector_t gyro;
	uint32_t sensor_time;
	uint8_t interrupt_tag;
	uint8_t skipped_frames;
	uint8_t config_change;
} bmi160_fifo_frame_t;

typedef struct
{
	uint8_t accel_passed;
	uint8_t gyro_passed;
} bmi160_self_test_result_t;

typedef struct
{
	bmi160_foc_target_t accel_x;
	bmi160_foc_target_t accel_y;
	bmi160_foc_target_t accel_z;
	uint8_t gyro;
	uint8_t enable_accel_offset;
	uint8_t enable_gyro_offset;
} bmi160_foc_config_t;

typedef struct
{
	int8_t accel_x;
	int8_t accel_y;
	int8_t accel_z;
	int16_t gyro_x;
	int16_t gyro_y;
	int16_t gyro_z;
} bmi160_offsets_t;

bmi160_config_t bmi160_default_config(void);
bmi160_status_t bmi160_init(bmi160_t *device, uint8_t address, const bmi160_config_t *config);
bmi160_status_t bmi160_soft_reset(bmi160_t *device);
bmi160_status_t bmi160_apply_config(bmi160_t *device, const bmi160_config_t *config);
bmi160_status_t bmi160_set_accel_power(bmi160_t *device, bmi160_accel_power_t power);
bmi160_status_t bmi160_set_gyro_power(bmi160_t *device, bmi160_gyro_power_t power);
bmi160_status_t bmi160_get_power_status(const bmi160_t *device, bmi160_power_status_t *status);
bmi160_status_t bmi160_read_registers(const bmi160_t *device, uint8_t reg, uint8_t *data, uint16_t length);
bmi160_status_t bmi160_write_registers(const bmi160_t *device, uint8_t reg, const uint8_t *data, uint16_t length);
bmi160_status_t bmi160_read_sample(const bmi160_t *device, bmi160_sample_t *sample);
bmi160_status_t bmi160_read_temperature(const bmi160_t *device, int32_t *temperature_millicelsius);
bmi160_status_t bmi160_read_error(const bmi160_t *device, uint8_t *error);
bmi160_status_t bmi160_read_interrupt_status(const bmi160_t *device, uint32_t *status);
void bmi160_scale_sample(const bmi160_t *device, const bmi160_sample_t *sample, bmi160_scaled_sample_t *scaled);

bmi160_status_t bmi160_configure_interrupt_pin(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_interrupt_pin_config_t *config);
bmi160_status_t bmi160_configure_any_motion(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_motion_config_t *config);
bmi160_status_t bmi160_configure_significant_motion(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_significant_motion_config_t *config);
bmi160_status_t bmi160_configure_no_motion(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_motion_config_t *config);
bmi160_status_t bmi160_configure_tap(const bmi160_t *device, bmi160_interrupt_pin_t pin,
		bmi160_interrupt_t type, const bmi160_tap_config_t *config);
bmi160_status_t bmi160_configure_orientation(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_orientation_config_t *config);
bmi160_status_t bmi160_configure_flat(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_flat_config_t *config);
bmi160_status_t bmi160_configure_low_g(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_low_g_config_t *config);
bmi160_status_t bmi160_configure_high_g(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, const bmi160_high_g_config_t *config);
bmi160_status_t bmi160_configure_data_ready(const bmi160_t *device, bmi160_interrupt_pin_t pin, uint8_t enable);
bmi160_status_t bmi160_configure_fifo_interrupt(const bmi160_t *device,
		bmi160_interrupt_t type, bmi160_interrupt_pin_t pin, uint8_t enable);
bmi160_status_t bmi160_configure_step_detector(const bmi160_t *device,
		bmi160_interrupt_pin_t pin, bmi160_step_mode_t mode, uint8_t enable);
bmi160_status_t bmi160_enable_step_counter(const bmi160_t *device, uint8_t enable);
bmi160_status_t bmi160_read_step_counter(const bmi160_t *device, uint16_t *steps);
bmi160_status_t bmi160_reset_step_counter(const bmi160_t *device);

bmi160_status_t bmi160_configure_fifo(bmi160_t *device, const bmi160_fifo_config_t *config);
bmi160_status_t bmi160_get_fifo_length(const bmi160_t *device, uint16_t *length);
bmi160_status_t bmi160_read_fifo(const bmi160_t *device, uint8_t *data, uint16_t capacity, uint16_t *length);
bmi160_status_t bmi160_flush_fifo(const bmi160_t *device);
void bmi160_fifo_parser_init(bmi160_fifo_parser_t *parser,
		const uint8_t *data, uint16_t length, const bmi160_fifo_config_t *config);
bmi160_status_t bmi160_fifo_next(bmi160_fifo_parser_t *parser, bmi160_fifo_frame_t *frame);

bmi160_status_t bmi160_run_self_test(bmi160_t *device, bmi160_self_test_result_t *result);
bmi160_status_t bmi160_start_foc(const bmi160_t *device, const bmi160_foc_config_t *config);
bmi160_status_t bmi160_foc_ready(const bmi160_t *device, uint8_t *ready);
bmi160_status_t bmi160_run_foc(const bmi160_t *device,
		const bmi160_foc_config_t *config, bmi160_offsets_t *offsets);
bmi160_status_t bmi160_get_offsets(const bmi160_t *device, bmi160_offsets_t *offsets);
bmi160_status_t bmi160_set_offsets(const bmi160_t *device,
		const bmi160_offsets_t *offsets, uint8_t accel_enable, uint8_t gyro_enable);

#endif
