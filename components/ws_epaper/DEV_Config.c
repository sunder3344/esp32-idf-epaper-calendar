/*****************************************************************************
* | File      	:   DEV_Config.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2020-02-19
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "DEV_Config.h"
#include "driver/gpio.h"
#include "driver/uart.h"

void GPIO_Config(void)
{
    // 定义一个 gpio_config_t 结构体用于批量配置 GPIO
    gpio_config_t io_conf = {}; // 初始化为0，确保所有成员都有默认值

    // --- 配置输出引脚 ---
    io_conf.intr_type = GPIO_INTR_DISABLE;        // 禁用中断
    io_conf.mode = GPIO_MODE_OUTPUT;              // 设置为输出模式
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // 禁用下拉电阻
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;     // 禁用上拉电阻
    // 设置要配置的输出引脚的位掩码
    io_conf.pin_bit_mask = (1ULL << EPD_RST_PIN) |
                           (1ULL << EPD_DC_PIN)  |
                           (1ULL << EPD_SCK_PIN) |
                           (1ULL << EPD_MOSI_PIN)|
                           (1ULL << EPD_CS_PIN);
    gpio_config(&io_conf); // 应用配置

    // --- 配置输入引脚 ---
    io_conf.intr_type = GPIO_INTR_DISABLE;        // 禁用中断
    io_conf.mode = GPIO_MODE_INPUT;               // 设置为输入模式
    // 对于 BUSY 引脚，根据你的 e-Paper 模块特性，可能需要启用内部上拉或下拉电阻
    // 假设它是一个低电平有效的信号，且没有外部上拉，则启用内部上拉
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;      // 启用上拉电阻
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // 禁用下拉电阻
    // 设置要配置的输入引脚的位掩码
    io_conf.pin_bit_mask = (1ULL << EPD_BUSY_PIN);
    gpio_config(&io_conf); // 应用配置

    // --- 设置初始电平 ---
    // 对应 digitalWrite(EPD_CS_PIN , HIGH);
    gpio_set_level(EPD_CS_PIN , 1); // 1 代表高电平
    // 对应 digitalWrite(EPD_SCK_PIN, LOW);
    gpio_set_level(EPD_SCK_PIN, 0); // 0 代表低电平
}

void GPIO_Mode(UWORD GPIO_Pin, UWORD Mode)
{
    if (Mode == 0) { // 对应 Arduino 的 INPUT
        gpio_set_direction(GPIO_Pin, GPIO_MODE_INPUT);
        // 通常输入引脚需要设置上下拉，这里可以根据你的需求添加默认设置
        // 例如：gpio_set_pull_mode(GPIO_Pin, GPIO_PULLUP_ONLY);
	} else { // 对应 Arduino 的 OUTPUT
		gpio_set_direction(GPIO_Pin, GPIO_MODE_OUTPUT);
	}
}
/******************************************************************************
function:	Module Initialize, the BCM2835 library and initialize the pins, SPI protocol
parameter:
Info:
******************************************************************************/
UBYTE DEV_Module_Init(void)
{
	//gpio
	GPIO_Config();

	//serial printf
	// 配置 UART0 参数
    uart_config_t uart_config = {
        .baud_rate = 115200,             // 设置波特率
        .data_bits = UART_DATA_8_BITS,   // 8位数据位
        .parity = UART_PARITY_DISABLE,   // 无校验位
        .stop_bits = UART_STOP_BITS_1,   // 1位停止位
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // 禁用硬件流控制
        .source_clk = UART_SCLK_APB,     // 使用 APB 时钟源 (ESP32 的默认时钟源)
    };

    // 安装 UART 驱动程序 (UART0)。
    // 第一个参数是 UART 端口号，通常 UART0 用于控制台。
    // 第二个参数是 RX 缓冲区大小。
    // 第三个参数是 TX 缓冲区大小 (如果为 0，表示使用默认的 TX FIFO)。
    // 其他参数用于 FreeRTOS 队列和中断，这里通常设为 NULL 和 0。
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);

    // 应用 UART 参数配置
    uart_param_config(UART_NUM_0, &uart_config);

    // 设置 UART 引脚。对于 UART0，通常 TX 是 GPIO1，RX 是 GPIO3。
    // 如果你没有更改过默认的引脚，这里可以保持不变。
    // UART_PIN_NO_CHANGE 表示不改变该引脚的默认映射。
    uart_set_pin(UART_NUM_0, GPIO_NUM_1, GPIO_NUM_3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

	// spi
	// SPI.setDataMode(SPI_MODE0);
	// SPI.setBitOrder(MSBFIRST);
	// SPI.setClockDivider(SPI_CLOCK_DIV4);
	// SPI.begin();

	return 0;
}

/******************************************************************************
function:
			SPI read and write
******************************************************************************/
void DEV_SPI_WriteByte(UBYTE data)
{
    // 对应 Arduino 的 digitalWrite(EPD_CS_PIN, GPIO_PIN_RESET);
    // 将片选线 (CS) 拉低，表示开始传输
    gpio_set_level(EPD_CS_PIN, GPIO_PIN_RESET);

    for (int i = 0; i < 8; i++)
    {
        // 对应 if ((data & 0x80) == 0) digitalWrite(EPD_MOSI_PIN, GPIO_PIN_RESET);
        //             else                    digitalWrite(EPD_MOSI_PIN, GPIO_PIN_SET);
        // 根据当前数据位的最高位设置 MOSI 引脚的电平
        if ((data & 0x80) == 0) {
            gpio_set_level(EPD_MOSI_PIN, GPIO_PIN_RESET); // 如果最高位是 0，MOSI 拉低
        } else {
            gpio_set_level(EPD_MOSI_PIN, GPIO_PIN_SET);   // 如果最高位是 1，MOSI 拉高
        }

        data <<= 1; // 将数据左移一位，准备发送下一位

        // 对应 digitalWrite(EPD_SCK_PIN, GPIO_PIN_SET);
        // 对应 digitalWrite(EPD_SCK_PIN, GPIO_PIN_RESET);
        // 拉高时钟线 (SCK)，然后拉低，完成一位数据的发送
        gpio_set_level(EPD_SCK_PIN, GPIO_PIN_SET);
        gpio_set_level(EPD_SCK_PIN, GPIO_PIN_RESET);
    }

    // 对应 Arduino 的 digitalWrite(EPD_CS_PIN, GPIO_PIN_SET);
    // 将片选线 (CS) 拉高，表示传输结束
    gpio_set_level(EPD_CS_PIN, GPIO_PIN_SET);
}

UBYTE DEV_SPI_ReadByte()
{
    UBYTE j = 0xff; // 初始化为全1，以便后续通过左移和按位或/与操作构建读取的字节

    // 对应 Arduino 的 GPIO_Mode(EPD_MOSI_PIN, 0);
    // 将 MOSI 引脚设置为输入模式，以便从从设备读取数据
    GPIO_Mode(EPD_MOSI_PIN, 0); // 0 表示 INPUT 模式 (根据你之前的转换)

    // 对应 Arduino 的 digitalWrite(EPD_CS_PIN, GPIO_PIN_RESET);
    // 将片选线 (CS) 拉低，表示开始传输
    gpio_set_level(EPD_CS_PIN, GPIO_PIN_RESET); // GPIO_PIN_RESET 对应低电平 (0)

    for (int i = 0; i < 8; i++)
    {
        j = (UBYTE)(j << 1); // 将当前已读取的位左移一位，为新读取的位腾出空间

        // 对应 Arduino 的 if (digitalRead(EPD_MOSI_PIN)) j = j | 0x01;
        //             else                            j = j & 0xfe;
        // 读取 MOSI (现在作为 MISO 使用) 引脚的电平
        if (gpio_get_level(EPD_MOSI_PIN)) { // 如果引脚为高电平 (1)
            j = (UBYTE)(j | 0x01);          // 将当前字节的最低位设置为 1
        } else {                            // 如果引脚为低电平 (0)
            j = (UBYTE)(j & 0xfe);          // 将当前字节的最低位设置为 0
        }

        // 对应 Arduino 的 digitalWrite(EPD_SCK_PIN, GPIO_PIN_SET);
        // 对应 Arduino 的 digitalWrite(EPD_SCK_PIN, GPIO_PIN_RESET);
        // 拉高时钟线 (SCK)，然后拉低，完成一位数据的读取
        gpio_set_level(EPD_SCK_PIN, GPIO_PIN_SET);   // GPIO_PIN_SET 对应高电平 (1)
        gpio_set_level(EPD_SCK_PIN, GPIO_PIN_RESET); // GPIO_PIN_RESET 对应低电平 (0)
    }

    // 对应 Arduino 的 digitalWrite(EPD_CS_PIN, GPIO_PIN_SET);
    // 将片选线 (CS) 拉高，表示传输结束
    gpio_set_level(EPD_CS_PIN, GPIO_PIN_SET); // GPIO_PIN_SET 对应高电平 (1)

    // 对应 Arduino 的 GPIO_Mode(EPD_MOSI_PIN, 1);
    // 将 MOSI 引脚重新设置为输出模式，恢复其原始状态
    GPIO_Mode(EPD_MOSI_PIN, 1); // 1 表示 OUTPUT 模式 (根据你之前的转换)

    return j; // 返回读取到的字节
}

void DEV_SPI_Write_nByte(UBYTE *pData, UDOUBLE len)
{
    for (int i = 0; i < len; i++)
        DEV_SPI_WriteByte(pData[i]);
}
