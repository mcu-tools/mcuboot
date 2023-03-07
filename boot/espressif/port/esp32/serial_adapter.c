/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <bootutil/bootutil_log.h>

#include <esp_rom_uart.h>
#include <esp_rom_gpio.h>
#include <esp_rom_sys.h>
#include <soc/uart_periph.h>
#include <soc/gpio_struct.h>
#include <soc/io_mux_reg.h>
#include <soc/rtc.h>
#include <hal/gpio_types.h>
#include <hal/gpio_ll.h>
#include <hal/uart_ll.h>
#include <hal/clk_gate_ll.h>

#ifdef CONFIG_ESP_SERIAL_BOOT_GPIO_DETECT
#define SERIAL_BOOT_GPIO_DETECT     CONFIG_ESP_SERIAL_BOOT_GPIO_DETECT
#else
#define SERIAL_BOOT_GPIO_DETECT     GPIO_NUM_5
#endif

#ifdef CONFIG_ESP_SERIAL_BOOT_GPIO_DETECT_VAL
#define SERIAL_BOOT_GPIO_DETECT_VAL     CONFIG_ESP_SERIAL_BOOT_GPIO_DETECT_VAL
#else
#define SERIAL_BOOT_GPIO_DETECT_VAL     1
#endif

#ifdef CONFIG_ESP_SERIAL_BOOT_DETECT_DELAY_S
#define SERIAL_BOOT_DETECT_DELAY_S     CONFIG_ESP_SERIAL_BOOT_DETECT_DELAY_S
#else
#define SERIAL_BOOT_DETECT_DELAY_S     5
#endif

#ifdef CONFIG_ESP_SERIAL_BOOT_GPIO_INPUT_TYPE
#define SERIAL_BOOT_GPIO_INPUT_TYPE    CONFIG_ESP_SERIAL_BOOT_GPIO_INPUT_TYPE
#else
// pull-down
#define SERIAL_BOOT_GPIO_INPUT_TYPE    0
#endif

#ifdef CONFIG_ESP_SERIAL_BOOT_UART_NUM
#define SERIAL_BOOT_UART_NUM    CONFIG_ESP_SERIAL_BOOT_UART_NUM
#else
#define SERIAL_BOOT_UART_NUM    ESP_ROM_UART_1
#endif

#ifdef CONFIG_ESP_SERIAL_BOOT_GPIO_RX
#define SERIAL_BOOT_GPIO_RX     CONFIG_ESP_SERIAL_BOOT_GPIO_RX
#else
#define SERIAL_BOOT_GPIO_RX     GPIO_NUM_8
#endif

#ifdef CONFIG_ESP_SERIAL_BOOT_GPIO_TX
#define SERIAL_BOOT_GPIO_TX     CONFIG_ESP_SERIAL_BOOT_GPIO_TX
#else
#define SERIAL_BOOT_GPIO_TX     GPIO_NUM_9
#endif

static uart_dev_t *serial_boot_uart_dev = (SERIAL_BOOT_UART_NUM == 0) ?
                                          &UART0 :
                                          &UART1;

void console_write(const char *str, int cnt)
{
    uint32_t tx_len;

    do {
        tx_len = uart_ll_get_txfifo_len(serial_boot_uart_dev);
    } while (tx_len < cnt);

    uart_ll_write_txfifo(serial_boot_uart_dev, (const uint8_t *)str, cnt);
}

int console_read(char *str, int cnt, int *newline)
{
    volatile uint32_t len = 0;
    volatile uint32_t read_len = 0;
    volatile bool stop = false;
    do {
        len = uart_ll_get_rxfifo_len(serial_boot_uart_dev);

        if (len) {
            for (uint32_t i = 0; i < len; i++) {
                /* Read the character from the RX FIFO */
                uart_ll_read_rxfifo(serial_boot_uart_dev, (uint8_t *)&str[read_len], 1);
                read_len++;
                if (read_len == cnt || str[read_len - 1] == '\n') {
                    stop = true;
                    break;
                }
            }
        }
        MCUBOOT_WATCHDOG_FEED();
        esp_rom_delay_us(1000);
    } while (!stop);

    *newline = (str[read_len - 1] == '\n') ? 1 : 0;
    return read_len;
}

int boot_console_init(void)
{
    BOOT_LOG_INF("Initializing serial boot pins");

    /* Enable GPIO for UART RX */
    esp_rom_gpio_pad_select_gpio(SERIAL_BOOT_GPIO_RX);
    esp_rom_gpio_connect_in_signal(SERIAL_BOOT_GPIO_RX,
                                   UART_PERIPH_SIGNAL(SERIAL_BOOT_UART_NUM, SOC_UART_RX_PIN_IDX),
                                   0);
    gpio_ll_input_enable(&GPIO, SERIAL_BOOT_GPIO_RX);

    /* Enable GPIO for UART TX */
    esp_rom_gpio_pad_select_gpio(SERIAL_BOOT_GPIO_TX);
    esp_rom_gpio_connect_out_signal(SERIAL_BOOT_GPIO_TX,
                                    UART_PERIPH_SIGNAL(SERIAL_BOOT_UART_NUM, SOC_UART_TX_PIN_IDX),
                                    0, 0);
    gpio_ll_output_enable(&GPIO, SERIAL_BOOT_GPIO_TX);

    uart_ll_set_sclk(serial_boot_uart_dev, UART_SCLK_APB);
    uart_ll_set_mode_normal(serial_boot_uart_dev);
    uart_ll_set_baudrate(serial_boot_uart_dev, 115200, UART_SCLK_APB);
    uart_ll_set_stop_bits(serial_boot_uart_dev, 1u);
    uart_ll_set_parity(serial_boot_uart_dev, UART_PARITY_DISABLE);
    uart_ll_set_rx_tout(serial_boot_uart_dev, 16);
    uart_ll_set_data_bit_num(serial_boot_uart_dev, UART_DATA_8_BITS);
    uart_ll_set_tx_idle_num(serial_boot_uart_dev, 0);
    uart_ll_set_hw_flow_ctrl(serial_boot_uart_dev, UART_HW_FLOWCTRL_DISABLE, 100);
    periph_ll_enable_clk_clear_rst(PERIPH_UART0_MODULE + SERIAL_BOOT_UART_NUM);

    uart_ll_txfifo_rst(serial_boot_uart_dev);
    uart_ll_rxfifo_rst(serial_boot_uart_dev);
    esp_rom_delay_us(50000);

    return 0;
}

bool boot_serial_detect_pin(void)
{
    bool detected = false;
    int pin_value = 0;

    esp_rom_gpio_pad_select_gpio(SERIAL_BOOT_GPIO_DETECT);
    gpio_ll_input_enable(&GPIO, SERIAL_BOOT_GPIO_DETECT);
    switch (SERIAL_BOOT_GPIO_INPUT_TYPE) {
        // Pull-down
        case 0:
            gpio_ll_pulldown_en(&GPIO, SERIAL_BOOT_GPIO_DETECT);
            break;
        // Pull-up
        case 1:
            gpio_ll_pullup_en(&GPIO, SERIAL_BOOT_GPIO_DETECT);
            break;
    }
    esp_rom_delay_us(50000);

    pin_value = gpio_ll_get_level(&GPIO, SERIAL_BOOT_GPIO_DETECT);
    detected = (pin_value == SERIAL_BOOT_GPIO_DETECT_VAL);
    esp_rom_delay_us(50000);

    if (detected) {
        if (SERIAL_BOOT_DETECT_DELAY_S > 0) {
            /* The delay time is an approximation */
            for (int i = 0; i < (SERIAL_BOOT_DETECT_DELAY_S * 100); i++) {
                esp_rom_delay_us(10000);
                pin_value = gpio_ll_get_level(&GPIO, SERIAL_BOOT_GPIO_DETECT);
                detected = (pin_value == SERIAL_BOOT_GPIO_DETECT_VAL);
                if (!detected) {
                    break;
                }
            }
        }
    }
    return detected;
}
