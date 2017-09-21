ifneq ($(wildcard $(PROJECT)/boot/zephyr/targets/$(BOARD).h),)
subdir-ccflags-y += -DMCUBOOT_TARGET_CONFIG='"$(BOARD).h"'
endif
subdir-ccflags-y += -I$(PROJECT)/boot/bootutil/include
subdir-ccflags-y += -I$(PROJECT)/boot/zephyr/include
subdir-ccflags-y += -I$(PROJECT)/boot/zephyr/targets

obj-y += main.o
obj-y += flash_map.o hal_flash.o os.o
# Legacy fallbacks used when the flash driver doesn't provide page
# layout support.
ifneq ($(CONFIG_FLASH_PAGE_LAYOUT),y)
obj-y += flash_map_legacy.o
endif
obj-y += keys.o

obj-y += ../bootutil/src/

obj-$(NEED_TINYCRYPT) += ../../ext/tinycrypt/lib/source/
include $(PROJECT)/ext/tinycrypt/lib/source/Makefile.inc
