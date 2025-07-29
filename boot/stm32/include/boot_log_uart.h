#ifndef BOOT_LOG_UART_H
#define BOOT_LOG_UART_H

#ifdef __cplusplus
extern "C" {
#endif

void boot_log_info(const char *msg, ...);
void boot_log_err(const char *msg, ...);
void boot_log_warn(const char *msg, ...);
void boot_log_debug(const char *msg, ...);

#ifdef __cplusplus
}
#endif

#endif // BOOT_LOG_UART_H
