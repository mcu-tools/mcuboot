#!/bin/bash
(set -o igncr) 2>/dev/null && set -o igncr; #keep this comment

echo_run() { echo "\$ ${@/eval/}" ; "$@" ; }

# set -vx

# parameters
# 1. secure/non-secure
# 2. path to app
# 3. app name
# 4. app type
# 5. path to keys
# 6. smif config file

# additional mcuboot makefile param
# 7. gcc toolchain path
# 8. path to policy
# 9. default application slot size
# 10. enable encryption
# 11. service application descriptor address

# Combined image configuration
LCS=$1
: ${LCS:="NORMAL_NO_SECURE"}

# Path to the image and image name for programming (the extension must be bin)
L1_USER_APP_BIN="$2/$3.bin"
: ${L1_USER_APP_BIN:="blinky.bin"}

if [ "$LCS" == "SECURE" ]; then
L1_USER_APP_BIN_SIGN="$2/$3.signed.bin"
: ${L1_USER_APP_BIN_SIGN:="blinky.signed.bin"}
fi

L1_USER_APP_ELF="$2/$3.elf"
: ${L1_USER_APP_ELF:="blinky.elf"}

FINAL_BIN_FILE="$2/$3.final.bin"
: ${FINAL_BIN_FILE:="blinky.final.bin"}

COMBINED_BIN_FILE="$2/$3.combined.bin"
: ${L1_USER_APP_BIN:="blinky.combined.bin"}

FINAL_HEX_FILE="$2/$3.final.hex"
: ${FINAL_HEX_FILE:="blinky.final.hex"}

AES_CTR_NONCE_FILE="$2/$3.signed_nonce.bin"

ENCRYPTED_MCUBOOT_BIN_FILE="$2/$3.signed_encrypted.bin"

TOC2_FILE="$2/toc2.bin"

L1_DESC_TEMP="$2/l1_app_desc_temp.bin"

L1_DESC_FILE="$2/l1_app_desc.bin"

L1_USER_APP_HEADER_FILE="$2/l1_user_app_header.bin"

APP_TYPE=$4
: ${APP_TYPE:="l1ram"}

PROVISION_PATH=$5
: ${PROVISION_PATH:="."}

# Path to the smif_crypto_cfg and its name for programming (the extension
# must be bin)
SMIF_CRYPTO_CFG=$6
: ${SMIF_CRYPTO_CFG:="NONE"}

# Full path to the default toolchain
if [ "$7" == "" ];then
    NM_TOOL=arm-none-eabi-nm
else
    NM_TOOL="$7"/bin/arm-none-eabi-nm
fi

POLICY_PATH=$8
: ${POLICY_PATH:="$PROVISION_PATH/policy/policy_secure.json"}

SLOT_SIZE=$9

if [ "${10}" == "1" ];then
    ENC_OPTION="--encrypt"
else
    ENC_OPTION=""
fi

SERVICE_APP_DESCR_ADDR=${11}
: ${SERVICE_APP_DESCR_ADDR:=0x0}

######################## Validate Input Args #################################
if ! [ -f $L1_USER_APP_BIN ]; then
	echo "ERROR: $L1_USER_APP_BIN not found"
	exit 1
fi

if ! [ -f $L1_USER_APP_ELF ]; then
	echo "ERROR: $L1_USER_APP_ELF not found"
	exit 1
fi

if ! [ -d $PROVISION_PATH ]; then
	echo "ERROR: $PROVISION_PATH not found"
	exit 1
fi

if [ "$TOOLCHAIN" != "ARM" ]; then
if ! [ -x $NM_TOOL ]; then
	echo "ERROR: $NM_TOOL not found"
	exit 1
fi
fi

if ! [ -x "$(command -v awk)" ]; then
	echo "ERROR: awk not found"
	exit 1
fi

# if ! [ -x "$(command -v print)" ]; then
# 	echo "ERROR: print not found"
# 	exit 1
# fi

if [ "$LCS" == "SECURE" ]; then
	if ! [ -x "$(command -v cysecuretools)" ]; then
		echo "ERROR: cysecuretools not found"
		exit 1
	fi
fi
######################### Generate TOC2 ########################################

# Hardcoded value bytes
TOC2_SIZE=16

# Hardcoded address in external memory
TOC2_ADDR=0x0

# TOC2 entries in hexadecimal
L1_APP_DESCR_ADDR=0x10
DEBUG_CERT_ADDR=0x0

# Convert to hex string (without 0x prefix)
TOC2_SIZE_HEX=$(printf "%08x" $TOC2_SIZE)
L1_APP_DESCR_ADDR_HEX=$(printf "%08x" $(expr $L1_APP_DESCR_ADDR))
SERVICE_APP_DESCR_ADDR_HEX=$(printf "%08x" $(expr $SERVICE_APP_DESCR_ADDR))
DEBUG_CERT_ADDR_HEX=$(printf "%08x" $(expr $DEBUG_CERT_ADDR))

# Write 4 bytes from hex string, LSB first
printf "\x"${TOC2_SIZE_HEX:6:2} >  $TOC2_FILE
printf "\x"${TOC2_SIZE_HEX:4:2} >> $TOC2_FILE
printf "\x"${TOC2_SIZE_HEX:2:2} >> $TOC2_FILE
printf "\x"${TOC2_SIZE_HEX:0:2} >> $TOC2_FILE

printf "\x"${L1_APP_DESCR_ADDR_HEX:6:2} >> $TOC2_FILE
printf "\x"${L1_APP_DESCR_ADDR_HEX:4:2} >> $TOC2_FILE
printf "\x"${L1_APP_DESCR_ADDR_HEX:2:2} >> $TOC2_FILE
printf "\x"${L1_APP_DESCR_ADDR_HEX:0:2} >> $TOC2_FILE

printf "\x"${SERVICE_APP_DESCR_ADDR_HEX:6:2} >> $TOC2_FILE
printf "\x"${SERVICE_APP_DESCR_ADDR_HEX:4:2} >> $TOC2_FILE
printf "\x"${SERVICE_APP_DESCR_ADDR_HEX:2:2} >> $TOC2_FILE
printf "\x"${SERVICE_APP_DESCR_ADDR_HEX:0:2} >> $TOC2_FILE

printf "\x"${DEBUG_CERT_ADDR_HEX:6:2} >> $TOC2_FILE
printf "\x"${DEBUG_CERT_ADDR_HEX:4:2} >> $TOC2_FILE
printf "\x"${DEBUG_CERT_ADDR_HEX:2:2} >> $TOC2_FILE
printf "\x"${DEBUG_CERT_ADDR_HEX:0:2} >> $TOC2_FILE

if [ ! -f "$TOC2_FILE" ]; then
    echo "Error: $TOC2_FILE does not exist." > /dev/tty
	exit -1
fi
######################### Generate L1_APP_DESCR ################################

# Hardcoded value bytes
L1_APP_DESCR_SIZE=28

# Placed after the TOC2
L1_APP_DESCR_ADDR=$(printf "0x%x" `expr $TOC2_SIZE`)
BOOT_STRAP_DST_ADDR=0x20004000
# L1_APP_DESCR entries in hexadecimal
if [ "$LCS" == "NORMAL_NO_SECURE" ]; then
	BOOT_STRAP_ADDR=0x50 # Fix address for un-signed image
else
	BOOT_STRAP_ADDR=0x30 # Fix address for signed image
fi

if [ "$APP_TYPE" == "l1ram" ]; then
	BOOT_STRAP_SIZE=`wc -c ${L1_USER_APP_BIN} | awk '{print $1}'`
	BOOT_STRAP_DST_ADDR_ELF=`${NM_TOOL} ${L1_USER_APP_ELF} | grep "__bootstrap_start_addr__" | awk '{print $1}'`
	BOOT_STRAP_DST_ADDR=$(printf "%d" $((16#$BOOT_STRAP_DST_ADDR_ELF)))

	if [ ! -f "$L1_USER_APP_BIN" ]; then
		echo "Error: $L1_USER_APP_BIN does not exist." > /dev/tty
		exit -1
	fi
else
	BOOT_STRAP_SIZE_ELF=`${NM_TOOL} ${L1_USER_APP_ELF} | grep "__bootstrap_size__" | awk '{print $1}'`
	BOOT_STRAP_SIZE=$(printf "%d" $((16#$BOOT_STRAP_SIZE_ELF)))
	BOOT_STRAP_DST_ADDR_ELF=`${NM_TOOL} ${L1_USER_APP_ELF} | grep "__bootstrap_start_addr__" | awk '{print $1}'`
	BOOT_STRAP_DST_ADDR=$(printf "%d" $((16#$BOOT_STRAP_DST_ADDR_ELF)))
if [ "$TOOLCHAIN" == "ARM" ]; then
# Reserving 0x3000 for bootstrap. This needs to be calculated based on the actual size
	BOOT_STRAP_SIZE=12288
fi
fi

# Convert to hex string (without 0x prefix)
L1_APP_DESCR_SIZE_HEX=$(printf "%08x" $L1_APP_DESCR_SIZE)
BOOT_STRAP_ADDR_HEX=$(printf "%08x" $BOOT_STRAP_ADDR)
BOOT_STRAP_DST_ADDR_HEX=$(printf "%08x" $BOOT_STRAP_DST_ADDR)
BOOT_STRAP_SIZE_HEX=$(printf "%08x" $BOOT_STRAP_SIZE)

if [ $BOOT_STRAP_SIZE_HEX == 00000000 ]; then
	echo "ERROR: in calculating bootstrap size"
	exit 1
fi

# Write 4 bytes from hex string, LSB first
printf "\x"${L1_APP_DESCR_SIZE_HEX:6:2} >  $L1_DESC_TEMP
printf "\x"${L1_APP_DESCR_SIZE_HEX:4:2} >> $L1_DESC_TEMP
printf "\x"${L1_APP_DESCR_SIZE_HEX:2:2} >> $L1_DESC_TEMP
printf "\x"${L1_APP_DESCR_SIZE_HEX:0:2} >> $L1_DESC_TEMP

printf "\x"${BOOT_STRAP_ADDR_HEX:6:2} >> $L1_DESC_TEMP
printf "\x"${BOOT_STRAP_ADDR_HEX:4:2} >> $L1_DESC_TEMP
printf "\x"${BOOT_STRAP_ADDR_HEX:2:2} >> $L1_DESC_TEMP
printf "\x"${BOOT_STRAP_ADDR_HEX:0:2} >> $L1_DESC_TEMP

printf "\x"${BOOT_STRAP_DST_ADDR_HEX:6:2} >> $L1_DESC_TEMP
printf "\x"${BOOT_STRAP_DST_ADDR_HEX:4:2} >> $L1_DESC_TEMP
printf "\x"${BOOT_STRAP_DST_ADDR_HEX:2:2} >> $L1_DESC_TEMP
printf "\x"${BOOT_STRAP_DST_ADDR_HEX:0:2} >> $L1_DESC_TEMP

printf "\x"${BOOT_STRAP_SIZE_HEX:6:2} >> $L1_DESC_TEMP
printf "\x"${BOOT_STRAP_SIZE_HEX:4:2} >> $L1_DESC_TEMP
printf "\x"${BOOT_STRAP_SIZE_HEX:2:2} >> $L1_DESC_TEMP
printf "\x"${BOOT_STRAP_SIZE_HEX:0:2} >> $L1_DESC_TEMP

if [ ! -f "$L1_DESC_TEMP" ]; then
	echo "Error: $L1_DESC_TEMP does not exist." > /dev/tty
	exit -1
fi

if [ "$SMIF_CRYPTO_CFG" == "NONE" ]; then
	for var in 0 1 2
	do
		SMIF_CRYPTO_CFG_HEX=00000000
		printf "\x"${SMIF_CRYPTO_CFG_HEX:6:2} >> $L1_DESC_TEMP
		printf "\x"${SMIF_CRYPTO_CFG_HEX:4:2} >> $L1_DESC_TEMP
		printf "\x"${SMIF_CRYPTO_CFG_HEX:2:2} >> $L1_DESC_TEMP
		printf "\x"${SMIF_CRYPTO_CFG_HEX:0:2} >> $L1_DESC_TEMP
	done

	`mv $L1_DESC_TEMP $L1_DESC_FILE`

	if [ ! -f "$L1_DESC_FILE" ]; then
		echo "Error: $L1_DESC_FILE does not exist." > /dev/tty
		exit -1
	fi
else
	if [ ! -f "$L1_DESC_TEMP.bin" ]; then
		echo "Error: $L1_DESC_TEMP.bin does not exist." > /dev/tty
		exit -1
	fi
	if [ ! -f "$SMIF_CRYPTO_CFG.bin" ]; then
		echo "Error: $SMIF_CRYPTO_CFG.bin does not exist." > /dev/tty
		exit -1
	fi

	`cat $L1_DESC_TEMP $SMIF_CRYPTO_CFG.bin > $L1_DESC_FILE
	 rm -f $L1_DESC_TEMP`

	if [ ! -f "$L1_DESC_FILE" ]; then
		echo "Error: $L1_DESC_FILE does not exist." > /dev/tty
		exit -1
	fi
	if [ -f "$L1_DESC_TEMP" ]; then
		echo "Error: $L1_DESC_TEMP has not been removed." > /dev/tty
		exit -1
	fi
fi

if [ ! -f "$TOC2_FILE" ]; then
	echo "Error: $TOC2_FILE does not exist." > /dev/tty
	exit -1
fi
if [ ! -f "$L1_DESC_FILE" ]; then
	echo "Error: $L1_DESC_FILE does not exist." > /dev/tty
	exit -1
fi

# 4 bytes of padding (encrypted data should be aligned to 0x10 boundary)
echo -en "\0\0\0\0" >> $L1_DESC_FILE

if [ "$LCS" == "NORMAL_NO_SECURE" ]; then

	number=0

	L1_USER_APP_HEADER_HEX=00000000
	# create 32 byte null header for bootstrap
	printf "\x"${L1_USER_APP_HEADER_HEX:6:2} > $L1_USER_APP_HEADER_FILE
	printf "\x"${L1_USER_APP_HEADER_HEX:4:2} >> $L1_USER_APP_HEADER_FILE
	printf "\x"${L1_USER_APP_HEADER_HEX:2:2} >> $L1_USER_APP_HEADER_FILE
	printf "\x"${L1_USER_APP_HEADER_HEX:0:2} >> $L1_USER_APP_HEADER_FILE

	if [ ! -f "$L1_USER_APP_HEADER_FILE" ]; then
		echo "Error: $L1_USER_APP_HEADER_FILE does not exist." > /dev/tty
		exit -1
	fi

	while [ $number -lt 7 ]
	do
		printf "\x"${L1_USER_APP_HEADER_HEX:6:2} >> $L1_USER_APP_HEADER_FILE
		printf "\x"${L1_USER_APP_HEADER_HEX:4:2} >> $L1_USER_APP_HEADER_FILE
		printf "\x"${L1_USER_APP_HEADER_HEX:2:2} >> $L1_USER_APP_HEADER_FILE
		printf "\x"${L1_USER_APP_HEADER_HEX:0:2} >> $L1_USER_APP_HEADER_FILE

		number=`expr $number + 1`
	done

	if [ ! -f "$L1_USER_APP_HEADER_FILE" ]; then
		echo "Error: $L1_USER_APP_HEADER_FILE does not exist." > /dev/tty
		exit -1
	fi
	if [ ! -f "$L1_USER_APP_BIN" ]; then
		echo "Error: $L1_USER_APP_BIN does not exist." > /dev/tty
		exit -1
	fi

	# Combining all images (toc2+l1_app_desc+l1_user_app_header+l1_user_app) to Final binary file
	`cat $TOC2_FILE $L1_DESC_FILE $L1_USER_APP_HEADER_FILE $L1_USER_APP_BIN > $FINAL_BIN_FILE`
elif [ "$LCS" == "SECURE" ]; then
	if [ ! -f "$L1_USER_APP_BIN" ]; then
		echo "Error: $L1_USER_APP_BIN does not exist." > /dev/tty
		exit -1
	fi

	# Sign l1 user app L1_USER_APP_BIN_SIGN
	cysecuretools -q -t cyw20829 -p $POLICY_PATH sign-image --image-format bootrom_next_app -i $L1_USER_APP_BIN -k 0 -o $L1_USER_APP_BIN_SIGN --slot-size $SLOT_SIZE --app-addr 0x08000030 $ENC_OPTION

	if [ ! -f "$L1_USER_APP_BIN_SIGN" ]; then
		echo "Error: $L1_USER_APP_BIN_SIGN has not been created." > /dev/tty
		exit -1
	fi

	# Combining all images (toc2+l1_app_desc+l1_user_app) to Final binary file of MCUBootApp
	if [ -z $ENC_OPTION ]; then
		cat $TOC2_FILE $L1_DESC_FILE $L1_USER_APP_BIN_SIGN > $FINAL_BIN_FILE
	else
		# Patching L1 app descriptor with AES-CTR Nonce
		dd seek=16 bs=1 count=12 conv=notrunc if=$AES_CTR_NONCE_FILE of=$L1_DESC_FILE >& /dev/null && \
		cat $TOC2_FILE $L1_DESC_FILE ${ENCRYPTED_MCUBOOT_BIN_FILE} > $FINAL_BIN_FILE
	fi
else
	echo "ERROR: Invalid LCS ($LCS) value"
	exit 1
fi

if [ ! -f "$FINAL_BIN_FILE" ]; then
	echo "Error: $FINAL_BIN_FILE does not exist." > /dev/tty
	exit -1
fi
