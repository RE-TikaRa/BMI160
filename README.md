# STM32F103C8T6 Keil 工程模板

这是一个面向 STM32F103C8T6 的 Keil MDK V5 工程模板，采用 CMSIS 和 STM32F10x Standard Peripheral Library。工程包含以下基础示例：

- 使用 `SysTick` 提供微秒、毫秒级阻塞延时。
- 使用 USART1 中断接收数据，并通过 `printf` 输出日志和接收数据。
- 周期性翻转 `PC13`，同时输出示例日志。
- Keil 工程已配置 72 MHz 系统时钟和 Hex 文件生成。

这份文档同时记录模板源目录和生成后工程的使用方式。生成工程时，`Project` 会被替换为输入的工程名；文中出现的 `Project.uvprojx`、`Project.hex` 等名称指的是当前模板的默认名称。

## 内容索引

- [工程配置](#工程配置)
- [快速开始](#快速开始)
- [硬件连接](#硬件连接)
- [示例行为](#示例行为)
- [USART1](#usart1)
- [工程目录](#工程目录)
- [编译与下载](#编译与下载)
- [启动链路与时钟](#启动链路与时钟)
- [内存布局与启动文件](#内存布局与启动文件)
- [Keil 工程细节](#keil-工程细节)
- [扩展工程](#扩展工程)
- [常见问题](#常见问题)
- [验证清单](#验证清单)
- [作为模板创建新工程](#作为模板创建新工程)
- [清理构建产物](#清理构建产物)
- [许可文件](#许可文件)

## 工程配置

| 项目 | 配置 |
| --- | --- |
| 目标器件 | `STM32F103C8`（C8T6 为封装型号） |
| 内核 | ARM Cortex-M3 |
| Keil Device Pack | `Keil.STM32F1xx_DFP.2.2.0` |
| ARM 编译器 | ARM Compiler 5.06 update 7（build 960） |
| 系统时钟 | 8 MHz HSE，经 PLL 配置为 72 MHz |
| Flash | `0x08000000`，64 KB |
| SRAM | `0x20000000`，20 KB |
| 编译器工程 | Keil MDK V5，ARM-ADS 工具链节点 |
| C 语言标准 | C99 编译选项已启用 |
| C 库 | 工程配置启用 MicroLIB |
| 调试信息 | `.axf`、调试信息和 Browse Information 已启用 |
| 启动栈 | 1 KB（`0x400`） |
| 启动堆 | 512 B（`0x200`） |
| 预处理宏 | `STM32F10X_MD,USE_STDPERIPH_DRIVER` |
| 外设库 | STM32F10x Standard Peripheral Library V3.6.4 |
| Hex 输出路径 | `project/MDK(V5)/Objects/Project.hex` |

`system_stm32f10x.c` 默认启用 `SYSCLK_FREQ_72MHz`，CMSIS 头文件中的 `HSE_VALUE` 默认是 8 MHz。若硬件使用其他外部晶振，需要同步调整时钟配置，否则延时和串口波特率都会产生偏差。

## 快速开始

### 只查看工程行为

1. 用 Keil MDK V5 打开 `project/MDK(V5)/Project.uvprojx`。
2. 确认目标器件显示为 `STM32F103C8`。
3. 执行 `Rebuild`，确认没有编译和链接错误。

### 下载并查看串口

1. 连接 SWD 调试器和开发板供电。
2. 连接 USB 转串口模块：`PA9 -> RX`、`PA10 -> TX`、`GND -> GND`。
3. 在 Keil 中选择实际调试器，执行 `Download`。
4. 打开对应 COM 口，设置为 115200-8-N-1。
5. 复位开发板，观察 `LED ON!` 和 `LED OFF!` 日志。
6. 发送一段短文本，停顿后观察 `data[长度] = ...` 回显。

快速流程只验证模板示例。新增外设、更换晶振或更换芯片后，应按照后面的时钟、内存、编译和验证章节重新检查相关配置。

## 硬件连接

### 最低硬件条件

运行当前示例至少需要：

1. 一块使用 STM32F103C8T6、且外部高速晶振与 `HSE_VALUE` 匹配的开发板。
2. 一个能够识别 `STM32F103C8` 的 SWD 下载器或调试器。
3. 一个 3.3 V 电平的 USB 转串口模块，用于查看 `printf` 输出和发送测试数据。

工程只描述 MCU 外设配置，没有绑定某一块具体开发板的原理图。`PC13` 是否连接板载 LED、LED 是高电平点亮还是低电平点亮，都需要以实际硬件为准。

### 当前代码使用的引脚

| 功能 | MCU 引脚 | 软件配置 | 说明 |
| --- | --- | --- | --- |
| USART1_TX | `PA9` | 50 MHz 复用推挽输出 | 接 USB 转串口模块的 RX |
| USART1_RX | `PA10` | 浮空输入 | 接 USB 转串口模块的 TX |
| 示例输出 | `PC13` | 50 MHz 推挽输出 | 主循环每 500 ms 改变一次电平 |
| 串口参考地 | `GND` | 共地 | 开发板与 USB 转串口模块必须共地 |

下载器的 SWDIO、SWCLK、NRST、3.3 V 和 GND 按调试器及开发板的丝印连接。烧录完成后，若需要从片内 Flash 启动，应将 BOOT0 等启动配置恢复到开发板说明中对应的 Flash 启动状态；本工程的向量表从 `0x08000000` 开始。

串口模块的 TX/RX 必须交叉连接。电平、供电和共地条件不满足时，即使固件成功编译，串口也可能没有可用输出。

## 示例行为

入口文件是 [`app/main.c`](app/main.c)。启动后依次完成以下操作：

1. 调用 `board_init()`，配置 `SysTick` 延时基准。
2. 调用 `uart1_init(115200)`，初始化 USART1。
3. 将 `PC13` 配置为 50 MHz 推挽输出。
4. 在主循环中每隔 500 ms 改变一次 `PC13` 电平，并输出：

   ```text
   LED ON!
   LED OFF!
   ```

代码中的日志对应 `GPIO_SetBits` 和 `GPIO_ResetBits` 两个动作，板载 LED 实际亮灭方向由具体开发板的 LED 极性决定。

对应的初始化顺序可以概括为：

```c
board_init();
uart1_init(115200);
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
GPIO_Init(GPIOC, &GPIO_InitStructure);
```

`PC13` 在进入循环前先被置低。进入循环后先置高并输出 `LED ON!`，等待 500 ms；再置低并输出 `LED OFF!`，等待 500 ms；最后调用 `uart1_read()` 查询并复制完整接收帧。

串口接收由中断持续进行，因此主循环读取接收标志的时间间隔最长接近一个完整主循环周期，即约 1 s，还会受 `printf` 发送时间影响。

当前主循环不是实时调度器，也没有任务队列。`printf` 发送时会逐字节等待 USART1 的发送数据寄存器空闲，串口输出量较大时，LED 周期和主循环轮询时间都会随发送时间延长。

## USART1

### 引脚与参数

| 信号 | 引脚 |
| --- | --- |
| TX | `PA9` |
| RX | `PA10` |
| GND | 与 USB 转串口模块共地 |

默认串口参数为 **115200-8-N-1**，无硬件流控。使用 USB 转串口模块时，将开发板 `PA9` 接模块 `RX`，`PA10` 接模块 `TX`。

### 接收方式

`bsp/uart/bsp_uart.c` 同时开启 `RXNE` 和 `IDLE` 中断：

- `RXNE` 中断把收到的字节写入 `u1_recv_buff`。
- 总线出现空闲后，`IDLE` 中断在缓冲区末尾写入 `\0`，保存帧长度并置位接收完成标志。
- 主循环调用 `uart1_read()` 将一帧复制到应用缓冲区，随后输出 `data[长度] = ...`。

这种实现以串口空闲间隔作为帧边界，不解析换行符。终端软件若自动附加 CR/LF，这些字节会保留在接收缓冲区中。接收缓冲区大小为 1024 字节，ISR 会将有效数据限制在 1023 字节以内，并通过溢出标志报告被丢弃的字节。

常用接口如下：

```c
void uart1_init(uint32_t baud);
uint8_t *uart1_get_data(void);
uint16_t uart1_get_data_length(void);
uint8_t uart1_get_overflow(void);
void uart1_receive_clear(void);
uint16_t uart1_read(uint8_t *data, uint16_t capacity, uint8_t *overflow);
```

`printf` 已在 `bsp_uart.c` 中重定向到 USART1，因此应用代码可以直接使用标准 C 输出函数。

### 初始化参数逐项对应

| 初始化项 | 代码配置 | 影响 |
| --- | --- | --- |
| 时钟 | 开启 AFIO、GPIOA、USART1 的 APB2 时钟 | 允许访问复用功能、GPIOA 和 USART1 |
| TX | `PA9`、`GPIO_Mode_AF_PP`、50 MHz | USART1 发送输出 |
| RX | `PA10`、`GPIO_Mode_IN_FLOATING` | USART1 接收输入 |
| 字长 | `USART_WordLength_8b` | 8 个数据位 |
| 停止位 | `USART_StopBits_1` | 1 个停止位 |
| 校验 | `USART_Parity_No` | 无校验位 |
| 工作模式 | `USART_Mode_Rx \| USART_Mode_Tx` | 同时启用收发 |
| 硬件流控 | `USART_HardwareFlowControl_None` | 不使用 RTS/CTS |
| 中断 | `USART_IT_RXNE`、`USART_IT_IDLE` | 接收字节和帧结束通知 |
| NVIC 优先级 | 抢占优先级 1，子优先级 1 | USART1 中断的优先级配置 |

`uart1_init()` 会先执行 `USART_DeInit(USART1)`，再按传入的波特率重新初始化。因此修改波特率时，通常只需要修改 `main.c` 中的调用参数，同时保持终端参数一致。

### 一帧数据的生命周期

```text
串口空闲
    │
    ├─ 收到一个字节：RXNE 中断
    │      └─ 写入 u1_recv_buff[u1_recv_length]
    │         然后 u1_recv_length 加一
    │
    ├─ 连续接收更多字节：重复 RXNE 中断
    │
    └─ 线路出现空闲：IDLE 中断
           ├─ 读取 USART1->SR 和 USART1->DR 清除 IDLE 条件
           ├─ 在当前索引写入 NUL 结束符
           └─ 将 u1_recv_flag 置为 1

主循环调用 uart1_read()
    ├─ u1_recv_flag 为 0：返回 0
    └─ u1_recv_flag 为 1：复制到调用方缓冲区，写入 NUL，清理当前帧状态
```

IDLE 是硬件检测到接收线路空闲后的事件，不等同于 `\r` 或 `\n`。例如终端发送 `abc` 后停顿，`abc` 会成为一帧；终端发送 `abc` 加回车换行时，回车和换行也会先进入缓冲区，再由 IDLE 结束整帧。

### 接口副作用与缓冲区所有权

| 接口 | 无数据时 | 有数据时 | 副作用 |
| --- | --- | --- | --- |
| `uart1_get_data()` | 返回 `NULL` | 返回 `u1_recv_buff` 首地址 | 立即清零 `u1_recv_length` 和 `u1_recv_flag` |
| `uart1_get_data_length()` | 返回当前帧长度 | 返回当前帧有效字节数 | 不改变接收状态 |
| `uart1_get_overflow()` | 返回 0 | 返回当前帧溢出标志 | 不改变接收状态 |
| `uart1_read()` | 返回 0 | 返回复制到调用方缓冲区的字节数 | 按 `capacity` 截断并报告溢出，然后清理当前帧状态 |
| `uart1_receive_clear()` | 无返回值 | 无返回值 | 清零长度、帧标志和溢出状态，不擦除缓冲区内容 |

`uart1_get_data()` 是保留的共享缓冲区接口；新代码应使用 `uart1_read()`，避免把内部缓冲区指针带出当前帧。当前实现没有环形缓冲区、帧队列或双缓冲；一帧尚未取走时，新接收字节会被丢弃并记录溢出，而不会覆盖已经标记完成的帧。

接收长度、帧长度和完成标志由中断与主循环共同访问，相关状态声明为 `volatile`。`uart1_read()` 在帧标志保持有效时复制数据，ISR 会暂存并丢弃后续字节；扩展为更高吞吐或多任务接收结构时，仍应改用环形缓冲区、DMA 或消息队列。

### 接收长度边界

`USART1_RECEIVE_LENGTH` 定义为 1024，最后一个字节需要留给 IDLE 中断写入的 `\0`。因此应用层应将单帧有效数据限制在 1023 字节以内。

当前 `uart1_irq_handler()` 会在写入前检查 `USART1_RECEIVE_LENGTH - 1`，最后一个字节保留给 NUL。超过 1023 字节的输入不会继续写出数组边界，`uart1_read()` 会通过 `overflow` 参数报告截断状态。

### `printf` 重定向

`fputc()` 的行为是：

1. 将一个字符写入 `USART1->DR`，实际调用为 `USART_SendData()`。
2. 轮询 `USART_FLAG_TXE`，等待发送数据寄存器可继续写入。
3. 返回当前字符。

它不会建立发送队列，也不会使用 DMA。发送函数是阻塞式的，调试日志不宜在高频中断中调用。`main.c` 中的日志显式携带 `\r\n`，库函数本身不会自动追加换行。

## 工程目录

```text
app/
  main.c                         应用入口和当前示例
board/
  board.c                       SysTick 延时实现
  board.h                       板级初始化与延时接口
bsp/uart/
  bsp_uart.c                    USART1 初始化、接收处理和 printf 重定向
  bsp_uart.h                    USART1 接口与接收缓冲区声明
module/
  stm32f10x_conf.h              标准外设库配置
  stm32f10x_it.c/.h             Cortex-M3 异常处理模板
libraries/
  CMSIS/                        Cortex-M3 CMSIS 与 STM32F10x 器件文件
  STM32F10x_StdPeriph_Driver/  ST 标准外设驱动库
project/MDK(V5)/
  Project.uvprojx               Keil MDK V5 工程文件
  Project.uvoptx                Keil 调试与选项配置
  Project.uvguix.Return         Keil 本地界面布局
  EventRecorderStub.scvd        Event Recorder 描述文件
  DebugConfig/
    Project_STM32F103C8_1.0.0.dbgconf
    Target_1_STM32F103C8_1.0.0.dbgconf
template.json                   工程创建器的模板元数据
.gitignore                     忽略 Keil 构建产物和本地界面文件
删除目标文件(用于打包备份).bat   清理 Keil 构建产物和本地界面状态
```

USART1 的向量入口位于 `module/stm32f10x_it.c`，其中调用 `bsp/uart/bsp_uart.c` 提供的 `uart1_irq_handler()`。这样可以把启动向量集中放在异常模块，把外设寄存器处理留在 BSP。

### 源文件职责

| 文件 | 责任 | 当前是否进入 Keil 编译组 |
| --- | --- | --- |
| `app/main.c` | 应用初始化、PC13 示例和主循环 | 是，`APP` |
| `board/board.c` | `SysTick` 时钟源设置和阻塞延时 | 是，`Board` |
| `board/board.h` | 板级初始化、延时函数声明 | 由源码包含 |
| `bsp/uart/bsp_uart.c` | USART1 初始化、接收处理、缓冲区和 `printf` 重定向 | 是，`BSP` |
| `bsp/uart/bsp_uart.h` | USART1 接口、接收缓冲区和安全读取 API | 由源码包含 |
| `module/stm32f10x_conf.h` | 标准外设库头文件开关和断言宏 | 通过包含路径使用 |
| `module/stm32f10x_it.c` | Cortex-M3 异常和 USART1 向量入口 | 是，`MODULE` |
| `module/stm32f10x_it.h` | 异常处理函数声明 | 由异常实现文件使用 |
| `system_stm32f10x.c` | 复位后的系统时钟和 `SystemCoreClock` | 是，`APP` |
| `startup_stm32f10x_md.s` | 中密度器件启动文件和向量表 | 是，`STARTUP` |

`stm32f10x_it.c` 已加入 `.uvprojx` 的 `MODULE` 组。新增异常或外设中断时，需要将相应源文件加入工程，并确保同一个向量只保留一个实现；USART1 的向量函数只负责转发到 `uart1_irq_handler()`。

## 编译与下载

1. 使用 Keil MDK V5 打开 `project/MDK(V5)/Project.uvprojx`。
2. 在 `Options for Target` 中确认器件为 `STM32F103C8`，并按实际调试器设置 Debug 和 Utilities 页面。
3. 执行 `Build` 或 `Rebuild`。
4. 编译成功后，构建文件位于 `project/MDK(V5)/Objects/`，Hex 输出名为 `Project.hex`。
5. 连接调试器后执行 `Download`，或进入 Debug 会话运行程序。

工程本身不包含 Keil 安装包、调试器驱动和 USB 转串口驱动；这些工具由本机开发环境提供。

### 命令行构建

本机的 UV4 位于 `C:\Keil_v5\UV4\UV4.exe`，可以在工程根目录执行：

```powershell
& "C:\Keil_v5\UV4\UV4.exe" -b "project\MDK(V5)\Project.uvprojx" -j0
```

构建日志写入 `project/MDK(V5)/Objects/Project.build_log.htm`。生成工程后，将命令中的工程文件和输出名称替换为新的项目名；`-j0` 使用当前工程的默认目标。

### 编译前检查

打开工程后，建议先在 `Options for Target` 中核对以下项目：

- `Device` 为 `STM32F103C8`。
- C/C++ 的预处理宏包含 `STM32F10X_MD` 和 `USE_STDPERIPH_DRIVER`。
- C/C++ 的 Include Paths 能找到 `app`、`board`、`bsp`、`module`、CMSIS 和标准外设库头文件目录。
- `Output` 中的目标名仍与工程目录一致；当前模板是 `Project`。
- `Create HEX File` 处于启用状态。
- `Debug` 和 `Utilities` 选择了实际连接的下载器，并使用适配 STM32F103C8 的 Flash Algorithm。

工程中的 Include Paths 使用相对于 `project/MDK(V5)` 的路径：

```text
..\..\app
..\..\board
..\..\bsp
..\..\libraries\CMSIS\CM3\CoreSupport
..\..\libraries\CMSIS\CM3\DeviceSupport\ST\STM32F10x
..\..\libraries\STM32F10x_StdPeriph_Driver\inc
..\..\module
..\..\bsp\uart
```

### 构建产物

工程配置启用了可执行映像、调试信息和 Hex 输出，默认输出目录为 `project/MDK(V5)/Objects/`：

| 文件 | 作用 |
| --- | --- |
| `Project.axf` | Keil 调试和符号信息使用的可执行映像 |
| `Project.hex` | 可供下载工具使用的 Intel HEX 文件 |
| 其他中间文件 | 由 Keil 按当前编译器和目标设置生成 |

使用模板创建器生成其他工程名后，上述 `Project` 会替换为输入的工程名，例如 `Sensor.axf` 和 `Sensor.hex`。不要在 `Objects` 中手工修改中间文件；修改应回到源码或 Keil 工程选项后重新构建。

### 下载和调试顺序

1. 给开发板和调试器提供稳定供电，连接 SWD 和复位线。
2. 打开 `.uvprojx`，先执行一次 `Rebuild`，确认工程文件和 Flash/SRAM 配置一致。
3. 在 `Debug` 页面选择调试器，在 `Utilities` 页面选择同一下载方式。
4. 点击 `Download`，观察 Keil 的擦除、编程和校验结果。
5. 复位目标板，打开串口终端，检查 LED 日志和 USART1 输出。
6. 需要单步时进入 Debug 会话，在 `main()`、`USART1_IRQHandler()`、`uart1_irq_handler()` 或 `delay_us()` 设置断点。

程序暂停在 `delay_us()` 时，CPU 是主动轮询 SysTick 的阻塞状态；程序暂停在 `uart1_irq_handler()` 时，可以直接观察 `u1_recv_length`、`u1_recv_frame_length`、`u1_recv_flag` 和 `u1_recv_overflow`。

## 串口验证

烧录后打开串口终端，选择 USART1 对应的 COM 口，设置为 115200-8-N-1。正常运行时可以看到 LED 日志持续输出：

```text
LED ON!
LED OFF!
LED ON!
LED OFF!
```

向开发板发送一段不超过 1023 字节的文本，等待串口出现空闲间隔后，终端会收到类似输出：

```text
data[5] = hello
```

串口接收以空闲间隔结束一帧，不以回车或换行结束。若终端启用了自动换行发送，回显内容会包含相应的控制字符。

## 修改入口

- 修改应用流程：编辑 `app/main.c`。
- 修改延时实现：编辑 `board/board.c`，接口声明位于 `board/board.h`。
- 修改串口引脚、波特率或接收逻辑：编辑 `bsp/uart/bsp_uart.c` 和 `bsp_uart.h`。
- 修改系统时钟：编辑 `libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/system_stm32f10x.c`，并检查 `stm32f10x.h` 中的 `HSE_VALUE`。
- 新增 `.c` 文件后，需要在 Keil 工程对应的组中手动加入文件；工程不会自动扫描目录。

更换 STM32F1 密度等级或具体型号时，还需要同时检查 Keil Device、预处理宏、启动文件、Flash/SRAM 地址范围和链接配置。

## 启动链路与时钟

### 复位到 `main()`

当前启动路径由 `startup_stm32f10x_md.s` 和 `system_stm32f10x.c` 共同完成：

```text
复位
  │
  ├─ 从 0x08000000 读取初始栈指针和 Reset_Handler 地址
  ├─ Reset_Handler 调用 SystemInit()
  │    ├─ 恢复 RCC 到复位状态
  │    ├─ 启动 HSE，配置 Flash 预取和等待周期
  │    ├─ 配置 PLL、AHB、APB1、APB2
  │    └─ 将向量表保持在 Flash 基地址
  ├─ 跳转到 ARM C 库 __main
  └─ 进入 app/main.c 的 main()
```

启动文件使用中密度器件版本 `startup_stm32f10x_md.s`。向量表中的 `USART1_IRQHandler` 指向 `module/stm32f10x_it.c`；该函数再调用 BSP 的 `uart1_irq_handler()`。其他未覆盖的向量仍使用启动文件提供的弱默认处理函数。

### 当前时钟树

`system_stm32f10x.c` 中启用了 `SYSCLK_FREQ_72MHz`，非 Connectivity Line 分支的时钟关系如下：

| 时钟节点 | 配置 | 频率 |
| --- | --- | ---: |
| HSE | 外部高速晶振，`HSE_VALUE` 默认 8 MHz | 8 MHz |
| PLL 输入 | HSE，不分频 | 8 MHz |
| PLL 倍频 | `PLL × 9` | 72 MHz |
| SYSCLK | PLL 输出 | 72 MHz |
| HCLK/AHB | `AHB DIV1` | 72 MHz |
| PCLK2/APB2 | `APB2 DIV1` | 72 MHz |
| PCLK1/APB1 | `APB1 DIV2` | 36 MHz |
| Flash | 预取开启，2 个等待周期 | - |
| SysTick 计数时钟 | `HCLK / 8` | 9 MHz |

HSE 启动超时常量为 `0x0500`。如果 HSE 没有在超时前就绪，库函数不会切换到 PLL，芯片会继续保持复位后的 HSI 时钟状态。

当前工程没有在 `main()` 中额外调用 `SystemCoreClockUpdate()`，也没有增加 HSE 错误处理。因此更换晶振或修改时钟宏后，应同时检查实际时钟、`SystemCoreClock`、延时和串口波特率。

### 修改系统时钟

需要修改时钟时，按以下顺序检查：

1. 修改 `stm32f10x.h` 中的 `HSE_VALUE`，使其与硬件晶振一致。
2. 在 `system_stm32f10x.c` 中选择或调整对应的 `SYSCLK_FREQ_*` 分支。
3. 确认 PLL 倍频不超过当前器件允许的最高频率。
4. 根据新频率调整 Flash 等待周期和 APB 分频。
5. 确认 `board_init()` 执行时 `SystemCoreClock` 已经反映新的 HCLK。
6. 重新编译，并使用串口输出和外部测量验证时钟变化造成的波特率、延时变化。

不要只修改 `SystemCoreClock` 的数值来伪造时钟配置；`SystemCoreClock` 是软件记录值，真正的时钟来自 RCC 寄存器和 PLL 配置。

### SysTick 延时接口

`board/board.h` 暴露了四个延时函数：

| 函数 | 实现 | 备注 |
| --- | --- | --- |
| `delay_us(us)` | SysTick 一次性倒计时 | 核心实现，按微秒计算装载值 |
| `delay_ms(ms)` | `delay_us(ms * 1000)` | 毫秒参数先换算为微秒 |
| `delay_1us(us)` | 调用 `delay_us(us)` | 当前是同一实现的别名 |
| `delay_1ms(ms)` | `delay_us(ms * 1000)` | 当前是同一实现的别名 |

`board_init()` 将 SysTick 时钟源设置为 `HCLK / 8`，并计算 `systick_us = SystemCoreClock / 8000000`。在默认 72 MHz HCLK 下，每微秒对应约 9 个 SysTick 计数单位。

`delay_us()` 通过读取 `SysTick->CTRL` 的使能位和 `COUNTFLAG` 轮询结束，然后关闭计数器并清零当前值。

源码对 `us == 1` 和其他数值使用不同的固定装载补偿值（分别减 8 和减 10），这是当前模板的周期校准方式。修改编译器、优化选项或系统时钟后，短延时的实际误差应重新测量。

这套延时会占用 CPU，不能提供后台计时，也不会在延时期间执行低功耗等待。`us * systick_us` 和 `ms * 1000` 都使用 32 位整数计算，超长延时需要改用定时器或其他时间基准，避免整数溢出。延时函数也不适合放在高优先级中断服务函数中。

## 内存布局与启动文件

### Keil 目标内存

当前 `.uvprojx` 和启动文件使用以下地址范围：

| 区域 | 起始地址 | 大小 | 用途 |
| --- | ---: | ---: | --- |
| IROM | `0x08000000` | `0x10000`（64 KB） | 程序代码、常量、向量表 |
| IRAM | `0x20000000` | `0x5000`（20 KB） | 全局变量、堆、栈和运行时数据 |
| 启动栈 | 由 `Stack_Mem` 提供 | `0x400`（1 KB） | 复位后主栈空间 |
| 启动堆 | 由 `Heap_Mem` 提供 | `0x200`（512 B） | C 库堆空间 |

工程没有配置外部 RAM，也没有使用自定义 Scatter File；链接器使用目标选项中的 IROM/IRAM 范围。程序、全局数据、栈和堆的总占用超过目标芯片容量时，Keil 会在链接阶段报告区域溢出。

### 向量表与异常处理

向量表位于 Flash 基地址，`VECT_TAB_OFFSET` 为 `0x0`。启动文件包含 Cortex-M3 核心异常和 STM32F10x Medium Density 外部中断向量。

未被应用覆盖的弱处理函数会进入死循环。因此某个外设意外触发中断时，调试器通常会停在 `Default_Handler` 或对应的弱异常处理函数。

`module/stm32f10x_it.c` 中的异常模板包含 HardFault、MemManage、BusFault、UsageFault 等处理函数，并已加入 `MODULE` 组。新增其他向量时，应保留每个向量的唯一实现。

## Keil 工程细节

### 工具链和编译选项

这些设置来自 `project/MDK(V5)/Project.uvprojx`，不是 README 的推测值：

| 设置 | 当前值 |
| --- | --- |
| 工程格式 | uVision Project Schema 2.1（`.uvprojx`） |
| 工具链节点 | ARM-ADS |
| ARM 编译器 | V5.06 update 7（build 960） |
| C++ 编译器 | 未启用，工程按 C 编译 |
| C99 | 已启用 |
| MicroLIB | 已启用 |
| 优化 | Keil 优化级别 1 |
| 警告级别 | `wLevel=2` |
| 调试信息 | 已启用 |
| Browse Information | 已启用 |
| Scatter File | 未指定，使用目标内存区域 |
| 编译前/后命令 | 未配置用户命令 |

工程使用 ARM Compiler 5 的 C 库重定向写法。`bsp_uart.c` 同时保留了非 MicroLIB 情况下的 `FILE`、`__stdout` 和 `_sys_exit()` 代码，因此切换 C 库时需要一起检查 `printf` 的链接结果。

### Keil 工程组

`.uvprojx` 当前有以下组：

| 组 | 内容 | 说明 |
| --- | --- | --- |
| `APP` | `system_stm32f10x.c`、`app/main.c` | 系统文件和应用入口 |
| `STARTUP` | `startup_stm32f10x_md.s` | 中密度器件启动文件 |
| `BSP` | `bsp/uart/bsp_uart.c` | USART1 驱动和接收处理 |
| `Board` | `board/board.c` | 板级延时实现 |
| `Driver` | `misc.c`、全部 STM32F10x 外设源文件、`core_cm3.c` | CMSIS 和标准外设库实现，共 24 个源文件 |
| `MODULE` | `module/stm32f10x_it.c` | Cortex-M3 异常和 USART1 向量入口 |
| `DOC` | `README.md` | 在 Keil 工程树中显示文档 |

Keil 不会因为文件出现在磁盘目录中就自动编译它。新增源文件后，需要在对应组上使用 `Add Existing Files to Group`，并检查头文件目录是否已经加入 Include Paths。

`DebugConfig` 下的两个 `.dbgconf` 文件都来自 STM32F1 调试配置模板，内容用于配置 `DBGMCU_CR`。它们不替代 `Debug`/`Utilities` 页面中的下载器选择；换用不同调试器时仍需在 Keil 中重新检查连接配置。

### 标准外设库配置

`module/stm32f10x_conf.h` 当前包含 ADC、BKP、CAN、CEC、CRC、DAC、DBGMCU、DMA、EXTI、FLASH、FSMC、GPIO、I2C、IWDG、PWR、RCC、RTC、SDIO、SPI、TIM、USART、WWDG 和 `misc` 的头文件。

这样可以直接使用标准外设库 API，但也意味着工程会编译 Driver 组中没有被当前示例调用的驱动源文件。

`USE_FULL_ASSERT` 目前处于注释状态，`assert_param()` 会展开为空操作。若需要参数断言：

1. 在 `stm32f10x_conf.h` 中启用 `USE_FULL_ASSERT`。
2. 提供 `assert_failed(uint8_t *file, uint32_t line)` 的实现。
3. 将错误信息输出到合适的调试通道。

只打开宏而不提供 `assert_failed()` 会导致链接错误。

## 扩展工程

### 增加 GPIO 或板级功能

推荐把与具体板卡有关的初始化放在 `board/`，把应用行为放在 `app/`。一个新的 GPIO 输出通常需要完成三件事：打开 GPIO 端口时钟、填写 `GPIO_InitTypeDef`、调用 `GPIO_Init()`。

```c
GPIO_InitTypeDef gpio;

RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
GPIO_StructInit(&gpio);
gpio.GPIO_Pin = GPIO_Pin_0;
gpio.GPIO_Mode = GPIO_Mode_Out_PP;
gpio.GPIO_Speed = GPIO_Speed_50MHz;
GPIO_Init(GPIOB, &gpio);
```

示例中的 `GPIOC` 时钟和 `PC13` 配置写在 `main.c`，因此换成另一块板卡时，应同步检查 LED 连接、有效电平和端口时钟，而不能只改日志文字。

### 修改 USART1 波特率

`uart1_init()` 接收一个 `uint32_t` 波特率参数。修改 `main.c` 中的调用即可改变初始化值：

```c
uart1_init(9600);
```

终端必须使用相同波特率。若输出出现乱码，优先检查三项：实际系统时钟、`HSE_VALUE` 和终端波特率。USART1 位于 APB2，时钟树改变后，标准外设库会根据新的 APB2 时钟计算 USART 分频值。

### 增加其他外设中断

新增中断外设时，建议按以下顺序处理：

1. 在对应源文件中开启外设和 GPIO 时钟。
2. 配置外设寄存器及中断源。
3. 使用 `NVIC_Init()` 配置中断通道、抢占优先级和子优先级。
4. 按 `startup_stm32f10x_md.s` 中的向量名称实现 `XXX_IRQHandler()`。
5. 将源文件加入 Keil 工程组。
6. 在中断函数中读取或清除触发标志，避免反复进入中断。

启动文件已经提供弱默认处理函数。应用实现同名强符号后会覆盖弱定义，但同一个中断不能在多个 `.c` 文件中重复定义。

### 使用 SysTick 时的约束

当前 `board.c` 将 SysTick 当作一次性倒计时器使用：

- 时钟源固定为 `HCLK / 8`。
- `delay_us()` 每次重新写入 `LOAD`、清零 `VAL`、开启计数器。
- 函数通过 `COUNTFLAG` 轮询结束，再关闭计数器。
- `SysTick_Handler()` 没有实现时间基准逻辑。

因此，不能在不改造 `board.c` 的情况下同时把 SysTick 当作周期性系统节拍使用。若需要 RTOS 节拍、毫秒计数或周期中断，应重新设计延时接口和 `SysTick_Handler()`，并重新检查所有依赖阻塞延时的代码。

### 新增源文件和头文件

新增模块时至少需要同步处理：

```text
1. 创建 .c/.h 文件
2. 在 Keil 的目标组中加入 .c 文件
3. 在需要的源文件中包含 .h
4. 如果头文件不在已有目录，增加 Include Path
5. Rebuild，确认没有重复符号和未解析符号
```

模板创建器只负责复制目录、替换名称和更新声明的文本文件，不会扫描新目录，也不会自动把源文件加入 `.uvprojx`。

### 更换 STM32F1 型号或容量

更换器件不能只修改 Keil 下拉框。至少需要逐项核对：

| 项目 | 当前模板 | 更换时需要检查 |
| --- | --- | --- |
| Keil Device | `STM32F103C8` | 目标型号和 Flash Algorithm |
| 宏 | `STM32F10X_MD` | LD、MD、HD、Connectivity Line 等密度宏 |
| 启动文件 | `startup_stm32f10x_md.s` | 对应密度和向量表 |
| IROM | 64 KB | 实际 Flash 起始地址和容量 |
| IRAM | 20 KB | 实际 SRAM 起始地址和容量 |
| HSE | 8 MHz 默认值 | 板卡晶振和 `HSE_VALUE` |
| 工程组 | 标准 F1 Driver 全量源文件 | 新型号所需的驱动文件和头文件 |

修改后需要重新检查链接内存、启动向量、时钟、下载算法和串口时序，不能沿用旧型号的所有地址和启动文件。

## 常见问题

### 编译阶段

**找不到 `stm32f10x.h` 或外设头文件**

检查 Include Paths 是否以 `project/MDK(V5)` 为基准，路径是否仍指向模板中的 `libraries`、`module` 和 `bsp` 目录。模板创建器生成新工程后，目录层级通常保持不变；若手动移动了 `.uvprojx`，需要重新整理相对路径。

**出现未定义的标准外设函数**

确认 `USE_STDPERIPH_DRIVER` 已加入预处理宏，并且 `Driver` 组仍包含对应的 `stm32f10x_*.c`。只包含头文件不会提供函数实现。

**`printf` 链接失败或输出函数缺失**

检查 MicroLIB 选项是否与当前工程一致，并确认 `bsp_uart.c` 已加入 `BSP` 组。若切换了 C 库，检查 `fputc()`、`__stdout`、`_sys_exit()` 相关代码和库入口要求。

**链接提示 IROM 或 IRAM 溢出**

先查看 `Objects` 中的映像大小，再检查是否误加入了多个启动文件、重复库或大型静态数组。不要为了消除错误而随意扩大 `.uvprojx` 中的 IROM/IRAM；容量应以实际芯片为准。

### 下载阶段

**Keil 找不到目标或无法擦写 Flash**

检查目标器件、Device Pack、调试器驱动、Flash Algorithm、供电、NRST 和 SWD 接线。当前工程的调试配置来自模板环境，换用其他调试器后需要在 `Debug` 和 `Utilities` 页面重新选择设备。

**下载成功但复位后没有运行**

检查 BOOT0 启动状态、复位线、电源电压和芯片是否被调试器保持在暂停状态。再确认程序确实链接到 `0x08000000`，而不是修改过的偏移地址。

### 运行阶段

**串口完全没有输出**

按以下顺序排查：

1. 串口模块 RX 是否接 `PA9`，TX 是否接 `PA10`。
2. 开发板与串口模块是否共地。
3. 终端是否选择了正确的 COM 口和 115200-8-N-1。
4. USB 转串口模块电平是否适合当前板卡。
5. 外部晶振是否与 `HSE_VALUE` 匹配，程序是否卡在启动或 HSE 等待阶段。

**日志乱码或时间间隔明显不对**

这通常表示软件记录的系统时钟与硬件实际时钟不一致。检查 HSE、PLL 倍频、Flash 等待周期、`SystemCoreClock` 和终端波特率；不要只在终端中反复修改波特率来掩盖时钟配置问题。

**能看到 LED 日志，但发送数据没有 `data[长度] = ...`**

发送后需要出现一段线路空闲，IDLE 中断才会把当前内容标记为完整帧。终端持续发送或自动重复发送时，可能一直没有形成预期的帧边界。还要检查发送数据是否超过 1023 字节、`overflow` 是否被置位，以及 USB 转串口模块是否真的连接到了 USART1。

**收到的数据偶尔错乱或被截断**

当前实现只有一个全局帧缓冲区，主循环每约 1 s 才查询一次完成标志，也没有环形队列。连续帧会被丢弃并标记溢出，长帧会被截断；需要更高吞吐时，应改为环形缓冲区、DMA 或明确的帧解析器。

**调试器停在 `Default_Handler` 或 HardFault**

检查是否误开了某个外设中断却没有实现对应处理函数，是否存在重复向量定义，是否在中断中访问了错误地址，以及是否发生了栈耗尽。可以先查看启动文件中当前向量对应的中断名称，再在该符号处设置断点。

## 验证清单

以下清单描述的是拿到工程后应执行的验证步骤，不代表模板已经完成实机验证。

### 编译验证

- [ ] Keil 能打开 `Project.uvprojx`，目标器件显示为 `STM32F103C8`。
- [ ] `Rebuild` 无编译错误和链接错误。
- [ ] `Objects` 中生成 `.axf` 和 `.hex`。
- [ ] IROM、IRAM、栈和堆占用没有超过目标范围。
- [ ] 工程名替换后，目标名、输出名、层名称与输入工程名一致。

### 下载验证

- [ ] 调试器能识别芯片并读取器件信息。
- [ ] 下载过程完成擦除、编程和校验。
- [ ] 复位后程序从 `0x08000000` 启动。
- [ ] 进入 Debug 后可以在 `main()` 设置断点并单步。

### 外设验证

- [ ] `PC13` 每约 500 ms 发生一次电平变化。
- [ ] USART1 持续输出 `LED ON!` 和 `LED OFF!`。
- [ ] 发送一段短文本并等待空闲间隔后，收到 `data[长度] = ...`。
- [ ] 发送带 CR/LF 的文本时，确认终端显示是否包含控制字符。
- [ ] 发送超过缓冲区上限的数据时，确认 `overflow` 被报告且程序没有写出接收数组边界。

## 作为模板创建新工程

仓库中的 `new-project` 创建器会读取此模板根目录的 `template.json`。该模板声明了 `Project` 在以下文件中的替换规则：

- `project/MDK(V5)/Project.uvprojx`
- `project/MDK(V5)/Project.uvoptx`
- `project/MDK(V5)/Project.uvguix.Return`
- `project/MDK(V5)/DebugConfig/*.dbgconf`

在 `C:\tools` 中运行：

```text
C:\tools\new-project.bat
```

选择 `STM32F103C8T6` 模板并输入工程名后，创建器会复制完整目录，并更新 Keil 工程中的目标名、输出名和层名称。创建器使用说明见 [`new-project.md`](../../new-project.md)。

### 当前模板元数据

`template.json` 的有效内容如下：

```json
{
  "name": "STM32F103C8T6",
  "description": "Complete STM32F103C8T6 Keil MDK V5 template with CMSIS, SPL, board support and USART1 debug I/O.",
  "nameReplacements": ["Project"],
  "textReplacements": [
    { "from": "<TargetName>Project</TargetName>", "to": "<TargetName>{project_name}</TargetName>" },
    { "from": "<OutputName>Project</OutputName>", "to": "<OutputName>{project_name}</OutputName>" },
    { "from": "<LayName>Project</LayName>", "to": "<LayName>{project_name}</LayName>" }
  ],
  "textExtensions": [".uvprojx", ".uvoptx", ".uvguix", ".dbgconf"]
}
```

字段的具体作用：

| 字段 | 当前作用 |
| --- | --- |
| `name` | 在模板选择界面显示 `STM32F103C8T6` |
| `description` | 在模板详情页显示说明文本 |
| `nameReplacements` | 将文件名和目录名中的 `Project` 替换为输入工程名 |
| `textReplacements` | 只替换三个明确的 Keil XML 标签内容 |
| `textExtensions` | 限定允许改写内容的文件扩展名 |

### 复制和替换顺序

创建器处理该模板时遵循以下顺序：

1. 扫描 `stm_project` 的一级子目录，将本目录识别为一个模板。
2. 复制模板的完整目录树。
3. 跳过模板根目录的 `template.json`，它只属于创建器元数据。
4. 对扩展名命中的文本文件应用 `textReplacements`。
5. 从目录树底部向上处理 `nameReplacements`，因此 `Project.uvguix.Return` 这类文件名也会被重命名。
6. 检查重命名冲突后写入最终工程目录。

`textExtensions` 会检查文件的全部后缀，因此 `Project.uvguix.Return` 仍会命中 `.uvguix`；`.dbgconf` 文件也会参与文本替换。源代码、README 和库文件不在当前模板的文本替换范围内。

例如输入工程名为 `MotorDemo`，核心结果会变为：

```text
project/MDK(V5)/MotorDemo.uvprojx
project/MDK(V5)/MotorDemo.uvoptx
project/MDK(V5)/MotorDemo.uvguix.Return
project/MDK(V5)/Objects/MotorDemo.hex
```

创建器不会改写 MCU 型号、内存大小、源码接口或目录结构，也不会自动添加新源文件。生成后仍应打开新的 `.uvprojx` 检查工程名、输出目录和调试器配置。

## 清理构建产物

打包或归档前可以运行工程根目录的：

```text
删除目标文件(用于打包备份).bat
```

该脚本只处理以下内容：

- `project/MDK(V5)/Listings/`
- `project/MDK(V5)/Objects/`
- `project/MDK(V5)/Project.uvgui.*`

源码、库文件和 `Project.uvprojx` 等主工程文件不会被脚本删除。

脚本先切换到自身所在目录，再按固定相对路径处理目标，因此可以从其他当前目录调用。目录删除失败或文件删除失败时会保留失败状态并返回非零退出码。清理前应关闭 Keil，避免 IDE 正在写入 `Objects`、`Listings` 或界面布局文件。

清理完成后重新打开工程，Keil 会在下一次构建时重新创建需要的输出目录和中间文件。该脚本不执行编译、不下载固件，也不会删除 `libraries`、`app`、`board`、`bsp`、`module` 或 `template.json`。

根目录的 `.gitignore` 只负责让构建产物不进入版本控制；模板创建器会复制实际目录中的文件。准备复制模板或打包归档前，仍应先清理 `Objects` 和 `Listings`。

## 许可文件

随工程附带的 CMSIS 和 STM32F10x 标准外设库保留各自的许可与说明文件：

- `libraries/CMSIS/License.doc`
- `libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/LICENSE.txt`
- `libraries/STM32F10x_StdPeriph_Driver/LICENSE.txt`

重新分发或二次修改库文件时，请一并保留这些文件。

工程中不同文件的版本注释来自 ST 的不同组件。启动文件和 `system_stm32f10x.c` 的注释版本为 V3.5.1，`stm32f10x_conf.h` 的模板注释版本为 V3.6.0，标准外设驱动头文件记录的库版本为 V3.6.4。

这里的版本信息用于追溯随工程保存的源码，不表示 Keil MDK 或 Device Pack 的版本。
