BLINKY := k64f_blinky
BLINKY2 := k64f_blinky2

SLINKY := k64f_slinky
SLINKY2 := k64f_slinky2

BOOT_RSA := k64f_boot_rsa
BOOT_RSA_PSS := k64f_boot_rsa_pss
BOOT_EC := k64f_boot_ec
BOOT_EC256 := k64f_boot_ec256
BOOT_RSA_EC := k64f_boot_rsa_ec
BOOT_RSA_VALIDATE0 := k64f_boot_rsa_validate0
BOOT_RSA_NOSWAP := k64f_boot_rsa_noswap

RSA_KEY_1 := key_rsa.pem
RSA_KEY_2 := key_rsa_2.pem

BLINKY2_IMG := bin/targets/$(BLINKY2)/app/apps/blinky/blinky.img

FLASH_ERASE := pyocd-flashtool -ce

NEWTMGR_CONN := --conn k64f
NEWTMGR_IMG := newtmgr $(NEWTMGR_CONN) image

all: build-apps build-mcuboot

build-blinky:
	@echo "* Building blinky for the primary slot... \c"
	@newt build -q $(BLINKY)
	@echo "ok"

build-blinky2:
	@echo "* Building blinky2 for the secondary slot... \c"
	@newt build -q $(BLINKY2)
	@echo "ok"

build-slinky:
	@echo "* Building slinky for the primary slot... \c"
	@newt build -q $(SLINKY)
	@echo "ok"

build-slinky2:
	@echo "* Building slinky2 for the secondary slot... \c"
	@newt build -q $(SLINKY2)
	@echo "ok"

build-boot-rsa:
	@echo "* Building mcuboot with RSA... \c"
	@newt build -q $(BOOT_RSA)
	@echo "ok"

build-boot-rsa-pss:
	@echo "* Building mcuboot with RSA/PSS... \c"
	@newt build -q $(BOOT_RSA_PSS)
	@echo "ok"

build-boot-ec:
	@echo "* Building mcuboot with EC... \c"
	@newt build -q $(BOOT_EC)
	@echo "ok"

build-boot-ec256:
	@echo "* Building mcuboot with EC256... \c"
	@newt build -q $(BOOT_EC256)
	@echo "ok"

# FIXME: multi-key signing does not work yet...
build-boot-rsa-ec:
	@echo "* Building mcuboot with RSA + EC... \c"
	@newt build -q $(BOOT_RSA_EC)
	@echo "ok"

build-boot-rsa-validate0:
	@echo "* Building mcuboot with primary slot validation... \c"
	@newt build -q $(BOOT_RSA_VALIDATE0)
	@echo "ok"

build-boot-rsa-noswap:
	@echo "* Building mcuboot with overwrite only upgrade... \c"
	@newt build -q $(BOOT_RSA_NOSWAP)
	@echo "ok"

build-apps: build-blinky build-blinky2 build-slinky build-slinky2

build-mcuboot: build-boot-rsa build-boot-rsa-pss build-boot-ec \
	           build-boot-ec256 build-boot-rsa-validate0 build-boot-rsa-noswap

clean:
	rm -rf bin/

.PHONY: all clean
