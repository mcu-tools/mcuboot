#ifndef __MCUBOOT_LOGGING_H__
#define __MCUBOOT_LOGGING_H__

#include "boot_log_uart.h"

#define MCUBOOT_LOG_ERR(...)  boot_log_err(__VA_ARGS__)
#define MCUBOOT_LOG_WRN(...)  boot_log_warn(__VA_ARGS__)
#define MCUBOOT_LOG_INF(...)  boot_log_info(__VA_ARGS__)
#define MCUBOOT_LOG_DBG(...)  boot_log_debug(__VA_ARGS__)

#define MCUBOOT_LOG_MODULE_DECLARE(...)
#define MCUBOOT_LOG_MODULE_REGISTER(...)

#endif /* __MCUBOOT_LOGGING_H__ */