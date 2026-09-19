# STM32F103C8T6 + BMI160 六轴例程

这是一个基于 STM32F10x Standard Peripheral Library 的 BMI160 完整示例工程。主程序通过 I2C1 初始化 BMI160，读取加速度、角速度、温度与传感器时间，并通过 USART1 输出换算后的数据。项目空白模板来自[嘉立创](https://wiki.lckfb.com/zh-hans/dkx-stm32f103c8t6/)。

驱动还提供电源模式、FIFO、数据就绪与运动中断、步数、自检、快速偏移校准和手动偏移接口。工程面向 Keil MDK V5 与 ARM Compiler 5。

## 默认配置

| 项目 | 配置 |
| --- | --- |
| MCU | STM32F103C8T6 |
| 系统时钟 | 72 MHz，外部 8 MHz HSE |
| BMI160 总线 | I2C1，100 kHz |
| SCL / SDA | PB6 / PB7 |
| BMI160 地址 | 0x68，SDO 接 GND |
| BMI160 接口选择 | CSB 接 3.3 V |
| 加速度计 | 100 Hz，Normal 带宽，±2 g |
| 陀螺仪 | 100 Hz，Normal 带宽，±500 °/s |
| 调试串口 | USART1，PA9 / PA10，115200-8-N-1 |
| 状态指示 | PC13，校准期间闪烁，完成后常亮，高电平点亮 |
| 工具链 | Keil MDK V5，ARM Compiler 5.06 update 7 |

BMI160 支持 0x68 与 0x69 两个 7 位 I2C 地址。SDO 接 GND 时使用 0x68，接 VDDIO 时使用 0x69。若改为 0x69，将 `main.c` 中的 `BMI160_I2C_ADDRESS_LOW` 改为 `BMI160_I2C_ADDRESS_HIGH`。

## 接线

| BMI160 模块 | STM32F103C8T6 | 说明 |
| --- | --- | --- |
| VCC / VDD | 3.3 V | 具体模块若带稳压器，仍应确认其电平设计 |
| VDDIO | 3.3 V | 裸芯片必须同时给 VDD 与 VDDIO 供电 |
| GND / GNDIO | GND | 共地 |
| SCL / SCx | PB6 | I2C1_SCL |
| SDA / SDx | PB7 | I2C1_SDA |
| CS / CSB | 3.3 V | 固定选择 I2C 模式 |
| SDO / SA0 | GND | 地址 0x68 |
| INT1 / INT2 | 按应用连接 | 使用中断接口时再连接到 EXTI 引脚 |

SCL 与 SDA 必须有外部上拉电阻。多数 BMI160 模块已经带上拉，裸芯片接法可从 4.7 kΩ 起配，并结合总线电容与实际波形调整。

## 快速开始

1. 用 Keil 打开 `project/MDK(V5)/BMI160.uvprojx`。
2. 按上表连接 BMI160、SWD 下载器和 USB 转串口模块。
3. 执行 Rebuild，确认 0 error、0 warning。
4. 下载固件并复位。
5. 打开 USART1 对应的串口，设置为 115200-8-N-1。

初始化成功后先输出芯片 ID：

```text
BMI160 ready, chip ID: 0xD1
BMI160 calibration: waiting for stillness
BMI160 calibration: sampling
BMI160 startup frame: A[mg] 773 -550 225
BMI160 gyro bias[mdps]: -61 122 -244
```

开机时不限制模块放置方向。程序先等待连续 0.5 秒稳定数据，再对后续 50 组六轴数据取平均。稳定条件为三轴角速度分别小于 5000 mdps，且相邻采样的三轴加速度变化分别小于 25 mg；等待上限为 10 秒。

平均重力方向用于建立新的 Z 轴，BMI160 原生 X 轴投影到水平面后建立新的 X 轴，再由叉乘得到 Y 轴。三轴平均角速度保存为软件偏置，后续每次读取时先逐轴扣除，再旋转到启动坐标系。

重力模长需处于 750-1250 mg，平均三轴角速度需分别小于 5000 mdps。稳定等待超时或最终平均值异常时，程序输出对应错误并保持错误闪烁。平均阶段允许单个加速度采样出现短暂波动。PC13 在等待稳定和取样期间闪烁，校准完成后保持点亮。

随后每 100 ms 输出一行：

```text
A[mg] 12 -8 1001  G[mdps] 61 -122 244  T[mC] 25125  time 123456
```

字段含义：

| 字段 | 单位 | 说明 |
| --- | --- | --- |
| `A[mg]` | mg | 启动坐标系中的三轴加速度，静止时约为 0、0、1000 mg |
| `G[mdps]` | mdps | 启动坐标系中的三轴角速度，1000 mdps 等于 1 °/s |
| `T[mC]` | m°C | 芯片内部温度 |
| `time` | 39 µs / LSB | BMI160 的 24 位传感器时间计数 |

初始化、校准或读取失败时，串口输出 `BMI160 error: n`，PC13 以 100 ms 间隔闪烁。错误码见后文。

## 工程结构

```text
app/
  main.c                       BMI160 初始化与周期读取示例
board/
  board.c/.h                   PC13 与 SysTick 阻塞延时
bsp/
  bmi160/
    bsp_bmi160.c/.h            BMI160 寄存器驱动与功能接口
  i2c/
    bsp_i2c.c/.h               STM32F1 I2C1 轮询传输
  uart/
    bsp_uart.c/.h              USART1、接收缓冲区与 printf 重定向
module/
  stm32f10x_conf.h             SPL 外设头文件配置
  stm32f10x_it.c/.h            Cortex-M3 异常与 USART1 中断入口
libraries/
  CMSIS/
  STM32F10x_StdPeriph_Driver/
project/MDK(V5)/
  BMI160.uvprojx               Keil 工程
  Objects/                     构建输出
  Listings/                    列表文件
```

Keil 不会扫描磁盘自动加入源码。新增 `.c` 文件时，需要同步更新 `BMI160.uvprojx` 的源文件组和 Include Paths。

## 初始化流程

`bmi160_init()` 执行以下过程：

1. 上电后等待 10 ms。
2. 读取 `CHIP_ID`，必须得到 0xD1。
3. 向 `CMD` 写入 0xB6，执行软件复位。
4. 写入加速度计 ODR、带宽和量程。
5. 写入陀螺仪 ODR、带宽和量程。
6. 将加速度计切换到 Normal，等待 5 ms。
7. 将陀螺仪切换到 Normal，等待 80 ms。
8. 读取 `ERR_REG` 与 `PMU_STATUS`，确认配置有效且两路传感器已经进入 Normal。

默认配置由 `bmi160_default_config()` 返回。也可以在初始化前修改：

```c
bmi160_config_t config = bmi160_default_config();

config.accel_odr = BMI160_ACCEL_ODR_200HZ;
config.accel_range = BMI160_ACCEL_RANGE_4G;
config.gyro_odr = BMI160_GYRO_ODR_200HZ;
config.gyro_range = BMI160_GYRO_RANGE_1000DPS;

status = bmi160_init(&bmi160, BMI160_I2C_ADDRESS_LOW, &config);
```

## 数据读取

`bmi160_read_sample()` 从 0x0C 开始连续读取 15 字节，一次取得陀螺仪、加速度计和传感器时间：

```c
bmi160_sample_t raw;
bmi160_scaled_sample_t scaled;

status = bmi160_read_sample(&bmi160, &raw);
if (status == BMI160_OK)
{
    bmi160_scale_sample(&bmi160, &raw, &scaled);
}
```

原始三轴数据为有符号 16 位整数。`bmi160_scale_sample()` 根据当前量程换算为 mg 和 mdps，不依赖浮点格式化。

`bmi160_scale_sample()` 保留 BMI160 原生坐标。主程序在输出前使用开机建立的旋转矩阵，同时旋转加速度和角速度。重力只能确定俯仰与横滚；绕重力方向的航向采用 BMI160 原生 X 轴在水平面上的投影作为基准。原生 X 轴接近竖直时改用原生 Y 轴。

温度单独读取：

```c
int32_t temperature_millicelsius;

status = bmi160_read_temperature(&bmi160, &temperature_millicelsius);
```

当返回原始值 0x8000 时，温度无效，接口返回 `BMI160_ERROR_NOT_READY`。

## 电源模式

```c
bmi160_set_accel_power(&bmi160, BMI160_ACCEL_NORMAL);
bmi160_set_accel_power(&bmi160, BMI160_ACCEL_LOW_POWER);
bmi160_set_accel_power(&bmi160, BMI160_ACCEL_SUSPEND);

bmi160_set_gyro_power(&bmi160, BMI160_GYRO_NORMAL);
bmi160_set_gyro_power(&bmi160, BMI160_GYRO_FAST_STARTUP);
bmi160_set_gyro_power(&bmi160, BMI160_GYRO_SUSPEND);
```

加速度计进入 Low Power 时，驱动同步设置 `ACC_CONF.acc_us`。陀螺仪从 Suspend 切换后按数据手册等待 80 ms。当前驱动采用阻塞式延时，接口返回后相应模式已经具备读取条件。

## 中断

驱动负责 BMI160 内部中断寄存器。STM32 的 GPIO、AFIO、EXTI 与 NVIC 配置取决于实际接线，应在 `board/` 和 `module/stm32f10x_it.c` 中完成。

先配置 INT1 或 INT2 的电气行为：

```c
bmi160_interrupt_pin_config_t pin = {
    .active_high = 1U,
    .open_drain = 0U,
    .edge_triggered = 1U,
    .input_enable = 0U,
    .latch = 0U
};

status = bmi160_configure_interrupt_pin(&bmi160, BMI160_INTERRUPT_1, &pin);
```

`latch` 对应 `INT_LATCH[3:0]`：

| 值 | 行为 |
| ---: | --- |
| 0x0 | 非锁存 |
| 0x1 至 0xE | 312.5 µs 至 2.56 s 的临时保持 |
| 0xF | 锁存 |

数据就绪中断：

```c
status = bmi160_configure_data_ready(
    &bmi160,
    BMI160_INTERRUPT_1,
    1U
);
```

任意运动：

```c
bmi160_motion_config_t motion = {
    .x = 1U,
    .y = 1U,
    .z = 1U,
    .duration = 0U,
    .threshold = 0x14U,
    .unfiltered = 0U
};

status = bmi160_configure_any_motion(
    &bmi160,
    BMI160_INTERRUPT_1,
    &motion
);
```

驱动提供以下中断配置接口：

- `bmi160_configure_any_motion()`
- `bmi160_configure_significant_motion()`
- `bmi160_configure_no_motion()`
- `bmi160_configure_tap()`
- `bmi160_configure_orientation()`
- `bmi160_configure_flat()`
- `bmi160_configure_low_g()`
- `bmi160_configure_high_g()`
- `bmi160_configure_data_ready()`
- `bmi160_configure_fifo_interrupt()`
- `bmi160_configure_step_detector()`

阈值、持续时间、迟滞等参数使用 BMI160 寄存器字段值。它们对应的物理量会随加速度量程、ODR 和具体功能变化，应按 Bosch 数据手册各中断章节计算。

将 `pin` 设为 `BMI160_INTERRUPT_NONE` 可关闭对应功能的映射和使能位。`bmi160_read_interrupt_status()` 一次读取四个 `INT_STATUS` 寄存器，返回 32 位原始状态。

## FIFO

BMI160 FIFO 容量为 1024 字节。驱动支持：

- 加速度、陀螺仪或二者同时写入。
- Header 与 Headerless 格式。
- 传感器时间帧。
- INT1 / INT2 标签。
- 加速度计与陀螺仪降采样。
- 过滤后或预过滤数据。
- FIFO watermark、full 中断。
- Header 数据帧解析。

配置示例：

```c
bmi160_fifo_config_t fifo = {
    .sensors = BMI160_FIFO_ACCEL_GYRO,
    .header = 1U,
    .sensor_time = 1U,
    .int1_tag = 0U,
    .int2_tag = 0U,
    .accel_downsample = 0U,
    .gyro_downsample = 0U,
    .filtered = 1U,
    .watermark = 240U
};

status = bmi160_configure_fifo(&bmi160, &fifo);
```

`watermark` 的接口单位是字节，驱动按 BMI160 的 4 字节单位写入寄存器。启用传感器时间时必须同时启用 Header。

读取和解析：

```c
uint8_t fifo_data[BMI160_FIFO_SIZE + BMI160_FIFO_OVERREAD];
uint16_t fifo_length;
bmi160_fifo_parser_t parser;
bmi160_fifo_frame_t frame;

status = bmi160_read_fifo(
    &bmi160,
    fifo_data,
    sizeof fifo_data,
    &fifo_length
);
if (status == BMI160_OK)
{
    bmi160_fifo_parser_init(&parser, fifo_data, fifo_length, &fifo);
    do
    {
        status = bmi160_fifo_next(&parser, &frame);
    } while (status == BMI160_OK && frame.type != BMI160_FIFO_FRAME_END);
}
```

启用传感器时间后，驱动在缓冲区容量足够时额外读取 25 字节，使 BMI160 返回传感器时间帧和 over-read 结束标记。

## 步数

```c
status = bmi160_configure_step_detector(
    &bmi160,
    BMI160_INTERRUPT_1,
    BMI160_STEP_NORMAL,
    1U
);
status = bmi160_enable_step_counter(&bmi160, 1U);
status = bmi160_read_step_counter(&bmi160, &steps);
status = bmi160_reset_step_counter(&bmi160);
```

可选模式为 `BMI160_STEP_NORMAL`、`BMI160_STEP_SENSITIVE` 和 `BMI160_STEP_ROBUST`。

## 自检

`bmi160_run_self_test()` 依次执行加速度计和陀螺仪自检：

```c
bmi160_self_test_result_t result;

status = bmi160_run_self_test(&bmi160, &result);
```

加速度计按数据手册切换到 ±8 g、1600 Hz，分别施加正负激励并检查三轴差值是否均大于 2 g。陀螺仪读取内建 BIST 结果。每项自检后都会软件复位并恢复调用前的量程、ODR 和带宽。

自检期间应保持模块静止，避免外部冲击。

## 启动校准与 FOC

主程序开机时先检测连续稳定状态，再使用任意静止姿态建立软件坐标系，并用后续 50 组平均角速度校准三轴陀螺仪软件偏置。启动流程不会改写 BMI160 的加速度计偏移寄存器。

BMI160 的加速度计 FOC 仍可用于已知朝向的专门校准。例如 X、Y 轴为 0 g，Z 轴朝上为 +1 g：

```c
bmi160_foc_config_t foc = {
    .accel_x = BMI160_FOC_ZERO_G,
    .accel_y = BMI160_FOC_ZERO_G,
    .accel_z = BMI160_FOC_POSITIVE_G,
    .gyro = 1U,
    .enable_accel_offset = 1U,
    .enable_gyro_offset = 1U
};
bmi160_offsets_t offsets;

status = bmi160_run_foc(&bmi160, &foc, &offsets);
```

`bmi160_start_foc()` 与 `bmi160_foc_ready()` 可用于非阻塞轮询。`bmi160_run_foc()` 最多等待 250 ms，并在 `STATUS.foc_rdy` 置位后读取偏移寄存器。偏移值也可以通过 `bmi160_get_offsets()` 和 `bmi160_set_offsets()` 读取或写入。

这些偏移在上电复位或软件复位后消失。工程没有自动写入 BMI160 NVM。

## I2C 传输

`bsp/i2c/bsp_i2c.c` 使用 STM32F1 硬件 I2C1：

- 7 位从机地址由接口传入。
- 寄存器读取使用 repeated START。
- 分别处理 1 字节、2 字节和多字节接收时序。
- 识别 NACK、总线错误、仲裁丢失、溢出和超时。
- 出错后恢复 ACK、NACK position 和错误标志。
- BMI160 写操作拆成单寄存器事务，并在事务后等待 1 ms，满足 Suspend 与 Low Power 下的写访问间隔。

I2C 状态由 `bsp_i2c_status_t` 返回；BMI160 层将它转换为 `BMI160_ERROR_I2C` 或 `BMI160_ERROR_TIMEOUT`。

## 错误码

| 错误码 | 含义 |
| --- | --- |
| `BMI160_OK` | 操作成功 |
| `BMI160_ERROR_ARGUMENT` | 空指针、枚举值或长度无效 |
| `BMI160_ERROR_I2C` | NACK、总线错误、仲裁丢失或溢出 |
| `BMI160_ERROR_TIMEOUT` | I2C、FOC 等待超时 |
| `BMI160_ERROR_DEVICE` | CHIP_ID 不是 0xD1 |
| `BMI160_ERROR_CONFIG` | BMI160 报告配置错误 |
| `BMI160_ERROR_NOT_READY` | 电源模式或数据尚未满足读取条件 |
| `BMI160_ERROR_BUFFER` | FIFO 缓冲区不足 |
| `BMI160_ERROR_FIFO` | FIFO 帧不完整或格式无效 |
| `BMI160_ERROR_SELF_TEST` | 加速度计或陀螺仪自检未通过 |

## 编译

本机 Keil 命令行：

```powershell
& "C:\Keil_v5\UV4\UV4.exe" -r "project\MDK(V5)\BMI160.uvprojx" -j0
```

zsh 中可执行：

```sh
C:/Keil_v5/UV4/UV4.exe -r 'project/MDK(V5)/BMI160.uvprojx' -j0
```

构建日志：

```text
project/MDK(V5)/Objects/BMI160.build_log.htm
```

输出文件：

```text
project/MDK(V5)/Objects/BMI160.axf
project/MDK(V5)/Objects/BMI160.hex
```

## 验证

编译验证：

- Keil 目标器件为 STM32F103C8。
- 预处理宏包含 `STM32F10X_MD` 与 `USE_STDPERIPH_DRIVER`。
- Rebuild 为 0 error、0 warning。
- `BMI160.axf` 与 `BMI160.hex` 已生成。
- IROM 与 IRAM 占用没有超过 64 KiB 和 20 KiB。

实机验证：

- 上电读取到 CHIP_ID 0xD1。
- `PMU_STATUS` 显示加速度计和陀螺仪均为 Normal。
- PC13 在开机校准期间闪烁，并在校准成功后保持点亮。
- 任意静止姿态启动后，输出加速度约为 X=0、Y=0、Z=1000 mg。
- 静止时角速度接近 0 mdps，转动模块时对应轴连续变化。
- 芯片温度输出处于合理范围。
- FIFO、中断、自检与 FOC 分别按接线和测试姿态验证。

成功编译只能证明源码与链接配置成立。I2C 电气连接、传感器数据和校准结果需要实机验证。

## 资料

- [BMI160 数据手册](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi160-ds000.pdf)
- [Bosch BMI160 SensorAPI](https://github.com/boschsensortec/BMI160_SensorAPI)
- [STM32F10x Standard Peripheral Library](https://www.st.com/)

## 许可

项目许可证为 Apache 2.0

[Apache License 2.0](LICENSE)



CMSIS 与 STM32F10x Standard Peripheral Library 保留各自的许可文件：

- `libraries/CMSIS/License.doc`
- `libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/LICENSE.txt`
- `libraries/STM32F10x_StdPeriph_Driver/LICENSE.txt`
