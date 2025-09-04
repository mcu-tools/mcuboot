#include "stm32wlxx_hal.h"
#include "bootutil_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart1;

static void boot_log_uart_send(const char *prefix, const char *msg, va_list args) {
    char buffer[128];
    int len = snprintf(buffer, sizeof(buffer), "\r\n[%s] ", prefix);
    vsnprintf(buffer + len, sizeof(buffer) - len, msg, args);


    if (HAL_UART_GetState(&huart1) == HAL_UART_STATE_READY) {
        HAL_UART_Transmit(&huart1, (uint8_t *)buffer, strlen(buffer), 10);
    }
}

void boot_log_info(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    boot_log_uart_send("INFO", msg, args);
    va_end(args);
}



void boot_log_err(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    boot_log_uart_send("ERR", msg, args);
    va_end(args);
}

void boot_log_warn(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    boot_log_uart_send("WARN", msg, args);
    va_end(args);
}

void boot_log_debug(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    boot_log_uart_send("DBG", msg, args);
    va_end(args);
}
