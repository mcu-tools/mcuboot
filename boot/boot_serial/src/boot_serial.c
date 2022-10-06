/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

#include "sysflash/sysflash.h"

#include "bootutil/bootutil_log.h"
#include "zcbor_encode.h"

#ifdef __ZEPHYR__
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/base64.h>
#include <hal/hal_flash.h>
#elif __ESPRESSIF__
#include <bootloader_utility.h>
#include <esp_rom_sys.h>
#include <rom/crc.h>
#include <endian.h>
#include <mbedtls/base64.h>
#else
#include <bsp/bsp.h>
#include <hal/hal_system.h>
#include <hal/hal_flash.h>
#include <os/endian.h>
#include <os/os_cputime.h>
#include <crc/crc16.h>
#include <base64/base64.h>
#endif /* __ZEPHYR__ */

#include <flash_map_backend/flash_map_backend.h>
#include <os/os.h>
#include <os/os_malloc.h>

#include <bootutil/image.h>
#include <bootutil/bootutil.h>

#include "boot_serial/boot_serial.h"
#include "boot_serial_priv.h"
#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_ERASE_PROGRESSIVELY
#include "bootutil_priv.h"
#endif

#ifdef MCUBOOT_ENC_IMAGES
#include "single_loader.h"
#endif

#include "serial_recovery_cbor.h"
#include "serial_recovery_echo.h"
#include "bootutil/boot_hooks.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#define BOOT_SERIAL_INPUT_MAX   512
#define BOOT_SERIAL_OUT_MAX     (128 * BOOT_IMAGE_NUMBER)

#ifdef __ZEPHYR__
/* base64 lib encodes data to null-terminated string */
#define BASE64_ENCODE_SIZE(in_size) ((((((in_size) - 1) / 3) * 4) + 4) + 1)

#define CRC16_INITIAL_CRC       0       /* what to seed crc16 with */
#define CRC_CITT_POLYMINAL 0x1021

#define ntohs(x) sys_be16_to_cpu(x)
#define htons(x) sys_cpu_to_be16(x)
#elif __ESPRESSIF__
#define BASE64_ENCODE_SIZE(in_size) ((((((in_size) - 1) / 3) * 4) + 4) + 1)
#define CRC16_INITIAL_CRC       0       /* what to seed crc16 with */

#define ntohs(x) be16toh(x)
#define htons(x) htobe16(x)

#define base64_decode mbedtls_base64_decode
#define base64_encode mbedtls_base64_encode
#endif

#if (BOOT_IMAGE_NUMBER > 1)
#define IMAGES_ITER(x) for ((x) = 0; (x) < BOOT_IMAGE_NUMBER; ++(x))
#else
#define IMAGES_ITER(x)
#endif

static char in_buf[BOOT_SERIAL_INPUT_MAX + 1];
static char dec_buf[BOOT_SERIAL_INPUT_MAX + 1];
const struct boot_uart_funcs *boot_uf;
static struct nmgr_hdr *bs_hdr;
static bool bs_entry;

static char bs_obuf[BOOT_SERIAL_OUT_MAX];

static void boot_serial_output(void);

static zcbor_state_t cbor_state[2];

void reset_cbor_state(void)
{
    zcbor_new_encode_state(cbor_state, 2, (uint8_t *)bs_obuf,
        (size_t)bs_obuf + sizeof(bs_obuf), 0);
}

/**
 * Function that processes MGMT_GROUP_ID_PERUSER mcumgr group and may be
 * used to process any groups that have not been processed by generic boot
 * serial implementation.
 *
 * @param[in] hdr -- the decoded header of mcumgr message;
 * @param[in] buffer -- buffer with first mcumgr message;
 * @param[in] len -- length of of data in buffer;
 * @param[out] *cs -- object with encoded response.
 *
 * @return 0 on success; non-0 error code otherwise.
 */
extern int bs_peruser_system_specific(const struct nmgr_hdr *hdr,
                                      const char *buffer,
                                      int len, zcbor_state_t *cs);

#define zcbor_tstr_put_lit_cast(state, string) \
	zcbor_tstr_encode_ptr(state, (uint8_t *)string, sizeof(string) - 1)

#ifndef MCUBOOT_USE_SNPRINTF
/*
 * Convert version into string without use of snprintf().
 */
static int
u32toa(char *tgt, uint32_t val)
{
    char *dst;
    uint32_t d = 1;
    uint32_t dgt;
    int n = 0;

    dst = tgt;
    while (val / d >= 10) {
        d *= 10;
    }
    while (d) {
        dgt = val / d;
        val %= d;
        d /= 10;
        if (n || dgt > 0 || d == 0) {
            *dst++ = dgt + '0';
            ++n;
        }
    }
    *dst = '\0';

    return dst - tgt;
}

/*
 * dst has to be able to fit "255.255.65535.4294967295" (25 characters).
 */
static void
bs_list_img_ver(char *dst, int maxlen, struct image_version *ver)
{
    int off;

    off = u32toa(dst, ver->iv_major);
    dst[off++] = '.';
    off += u32toa(dst + off, ver->iv_minor);
    dst[off++] = '.';
    off += u32toa(dst + off, ver->iv_revision);
    dst[off++] = '.';
    off += u32toa(dst + off, ver->iv_build_num);
}
#else
/*
 * dst has to be able to fit "255.255.65535.4294967295" (25 characters).
 */
static void
bs_list_img_ver(char *dst, int maxlen, struct image_version *ver)
{
   snprintf(dst, maxlen, "%hu.%hu.%hu.%u", (uint16_t)ver->iv_major,
            (uint16_t)ver->iv_minor, ver->iv_revision, ver->iv_build_num);
}
#endif /* !MCUBOOT_USE_SNPRINTF */

/*
 * List images.
 */
static void
bs_list(char *buf, int len)
{
    struct image_header hdr;
    uint8_t tmpbuf[64];
    uint32_t slot, area_id;
    const struct flash_area *fap;
    uint8_t image_index;

    zcbor_map_start_encode(cbor_state, 1);
    zcbor_tstr_put_lit_cast(cbor_state, "images");
    zcbor_list_start_encode(cbor_state, 5);
    image_index = 0;
    IMAGES_ITER(image_index) {
        for (slot = 0; slot < 2; slot++) {
            area_id = flash_area_id_from_multi_image_slot(image_index, slot);
            if (flash_area_open(area_id, &fap)) {
                continue;
            }

            int rc = BOOT_HOOK_CALL(boot_read_image_header_hook,
                                    BOOT_HOOK_REGULAR, image_index, slot, &hdr);
            if (rc == BOOT_HOOK_REGULAR)
            {
                flash_area_read(fap, 0, &hdr, sizeof(hdr));
            }

            fih_int fih_rc = FIH_FAILURE;

            if (hdr.ih_magic == IMAGE_MAGIC)
            {
                BOOT_HOOK_CALL_FIH(boot_image_check_hook,
                                   fih_int_encode(BOOT_HOOK_REGULAR),
                                   fih_rc, image_index, slot);
                if (fih_eq(fih_rc, BOOT_HOOK_REGULAR))
                {
#ifdef MCUBOOT_ENC_IMAGES
                    if (slot == 0 && IS_ENCRYPTED(&hdr)) {
                        /* Clear the encrypted flag we didn't supply a key
                        * This flag could be set if there was a decryption in place
                        * performed before. We will try to validate the image without
                        * decryption by clearing the flag in the heder. If
                        * still encrypted the validation will fail.
                        */
                        hdr.ih_flags &= ~(ENCRYPTIONFLAGS);
                    }
#endif
                    FIH_CALL(bootutil_img_validate, fih_rc, NULL, 0, &hdr, fap, tmpbuf, sizeof(tmpbuf),
                                    NULL, 0, NULL);
                }
            }

            flash_area_close(fap);

            if (fih_not_eq(fih_rc, FIH_SUCCESS)) {
                continue;
            }

            zcbor_map_start_encode(cbor_state, 20);

#if (BOOT_IMAGE_NUMBER > 1)
            zcbor_tstr_put_lit_cast(cbor_state, "image");
            zcbor_uint32_put(cbor_state, image_index);
#endif

            zcbor_tstr_put_lit_cast(cbor_state, "slot");
            zcbor_uint32_put(cbor_state, slot);
            zcbor_tstr_put_lit_cast(cbor_state, "version");

            bs_list_img_ver((char *)tmpbuf, sizeof(tmpbuf), &hdr.ih_ver);
            zcbor_tstr_encode_ptr(cbor_state, tmpbuf, strlen((char *)tmpbuf));
            zcbor_map_end_encode(cbor_state, 20);
        }
    }
    zcbor_list_end_encode(cbor_state, 5);
    zcbor_map_end_encode(cbor_state, 1);
    boot_serial_output();
}

#ifdef MCUBOOT_ERASE_PROGRESSIVELY

/** Erases range of flash, aligned to sector size
 *
 * Function will erase all sectors withing [start, end] range; it does not check
 * the @p start for alignment, and it will use @p end to find boundaries of las
 * sector to erase. Function returns offset of the first byte past the last
 * erased sector, so basically offset of next sector to be erased if needed.
 * The function is intended to be called iteratively with previously returned
 * offset as @p start.
 *
 * @param   start starting offset, aligned to sector offset;
 * @param   end ending offset, maybe anywhere within sector;
 *
 * @retval On success: offset of the first byte past last erased sector;
 *         On failure: -EINVAL.
 */
static off_t erase_range(const struct flash_area *fap, off_t start, off_t end)
{
    struct flash_sector sect;
    size_t size;
    int rc;

    if (end >= flash_area_get_size(fap)) {
        return -EINVAL;
    }

    if (end < start) {
        return start;
    }

    if (flash_area_sector_from_off(end, &sect)) {
        return -EINVAL;
    }

    size = flash_sector_get_off(&sect) + flash_sector_get_size(&sect) - start;
    BOOT_LOG_INF("Erasing range 0x%jx:0x%jx", (intmax_t)start,
		 (intmax_t)(start + size - 1));

    rc = flash_area_erase(fap, start, size);
    if (rc != 0) {
        BOOT_LOG_ERR("Error %d while erasing range", rc);
        return -EINVAL;
    }

    return start + size;
}
#endif

/*
 * Image upload request.
 */
static void
bs_upload(char *buf, int len)
{
    static size_t img_size;             /* Total image size, held for duration of upload */
    static uint32_t curr_off;           /* Expected current offset */
    const uint8_t *img_chunk = NULL;    /* Pointer to buffer with received image chunk */
    size_t img_chunk_len = 0;           /* Length of received image chunk */
    size_t img_chunk_off = SIZE_MAX;    /* Offset of image chunk within image  */
    uint8_t rem_bytes;                  /* Reminder bytes after aligning chunk write to
                                         * to flash alignment */
    int img_num;
    size_t img_size_tmp = SIZE_MAX;     /* Temp variable for image size */
    const struct flash_area *fap = NULL;
    int rc;
#ifdef MCUBOOT_ERASE_PROGRESSIVELY
    static off_t not_yet_erased = 0;    /* Offset of next byte to erase; writes to flash
                                         * are done in consecutive manner and erases are done
                                         * to allow currently received chunk to be written;
                                         * this state variable holds information where last
                                         * erase has stopped to let us know whether erase
                                         * is needed to be able to write current chunk.
                                         */
    static struct flash_sector status_sector;
#endif

    img_num = 0;

    /*
     * Expected data format.
     * {
     *   "image":<image number in a multi-image set (OPTIONAL)>
     *   "data":<image data>
     *   "len":<image len>
     *   "off":<current offset of image data>
     * }
     */

    struct Upload upload;
    size_t decoded_len;
    uint_fast8_t result = cbor_decode_Upload((const uint8_t *)buf, len, &upload, &decoded_len);

    if ((result != ZCBOR_SUCCESS) || (len != decoded_len)) {
        goto out_invalid_data;
    }

    for (int i = 0; i < upload._Upload_members_count; i++) {
        struct Member_ *member = &upload._Upload_members[i]._Upload_members;
        switch(member->_Member_choice) {
            case _Member_image:
                img_num = member->_Member_image;
                break;
            case _Member_data:
                img_chunk = member->_Member_data.value;
                img_chunk_len = member->_Member_data.len;
                break;
            case _Member_len:
                img_size_tmp = member->_Member_len;
                break;
            case _Member_off:
                img_chunk_off = member->_Member_off;
                break;
            case _Member_sha:
            default:
                /* Nothing to do. */
                break;
        }
    }

    if (img_chunk_off == SIZE_MAX || img_chunk == NULL) {
        /*
         * Offset must be set in every block.
         */
        goto out_invalid_data;
    }

#if !defined(MCUBOOT_SERIAL_DIRECT_IMAGE_UPLOAD)
    rc = flash_area_open(flash_area_id_from_multi_image_slot(img_num, 0), &fap);
#else
    rc = flash_area_open(flash_area_id_from_direct_image(img_num), &fap);
#endif
    if (rc) {
        rc = MGMT_ERR_EINVAL;
        goto out;
    }

    if (img_chunk_off == 0) {
        /* Receiving chunk with 0 offset resets the upload state; this basically
         * means that upload has started from beginning.
         */
        const size_t area_size = flash_area_get_size(fap);

        curr_off = 0;
#ifdef MCUBOOT_ERASE_PROGRESSIVELY
        /* Get trailer sector information; this is done early because inability to get
         * that sector information means that upload will not work anyway.
         * TODO: This is single occurrence issue, it should get detected during tests
         * and fixed otherwise you are deploying broken mcuboot.
         */
        if (flash_area_sector_from_off(boot_status_off(fap), &status_sector)) {
            rc = MGMT_ERR_EUNKNOWN;
            BOOT_LOG_ERR("Unable to determine flash sector of the image trailer");
            goto out;
         }
#endif


#if defined(MCUBOOT_VALIDATE_PRIMARY_SLOT_ONCE)
        /* We are using swap state at end of flash area to store validation
         * result. Make sure the user cannot write it from an image to skip validation.
         */
        if (img_size_tmp > (area_size - BOOT_MAGIC_SZ)) {
            goto out_invalid_data;
        }
#else
        if (img_size_tmp > area_size) {
            goto out_invalid_data;
        }

#endif

#ifndef MCUBOOT_ERASE_PROGRESSIVELY
        /* Non-progressive erase erases entire image slot when first chunk of
         * an image is received.
         */
        rc = flash_area_erase(fap, 0, area_size);
        if (rc) {
            goto out_invalid_data;
        }
#else
        not_yet_erased = 0;
#endif

        img_size = img_size_tmp;
    } else if (img_chunk_off != curr_off) {
        /* If received chunk offset does not match expected one jump, pretend
         * success and jump to out; out will respond to client with success
         * and request the expected offset, held by curr_off.
         */
        rc = 0;
        goto out;
    } else if (curr_off + img_chunk_len > img_size) {
        rc = MGMT_ERR_EINVAL;
        goto out;
    }

#ifdef MCUBOOT_ERASE_PROGRESSIVELY
    /* Progressive erase will erase enough flash, aligned to sector size,
     * as needed for the current chunk to be written.
     */
    not_yet_erased = erase_range(fap, not_yet_erased,
                                 curr_off + img_chunk_len - 1);

    if (not_yet_erased < 0) {
        rc = MGMT_ERR_EINVAL;
        goto out;
    }
#endif

    /* Writes are aligned to flash write alignment, so may drop a few bytes
     * from the end of the buffer; we will request these bytes again with
     * new buffer by responding with request for offset after the last aligned
     * write.
     */
    rem_bytes = img_chunk_len % flash_area_align(fap);
    img_chunk_len -= rem_bytes;

    if (curr_off + img_chunk_len + rem_bytes < img_size) {
        rem_bytes = 0;
    }

    BOOT_LOG_INF("Writing at 0x%x until 0x%x", curr_off, curr_off + img_chunk_len);
    /* Write flash aligned chunk, note that img_chunk_len now holds aligned length */
    rc = flash_area_write(fap, curr_off, img_chunk, img_chunk_len);
    if (rc == 0 && rem_bytes) {
        /* Non-zero rem_bytes means that last chunk needs alignment; the aligned
         * part, in the img_chunk_len - rem_bytes count bytes, has already been
         * written by the above write, so we are left with the rem_bytes.
         */
        uint8_t wbs_aligned[BOOT_MAX_ALIGN];

        memset(wbs_aligned, flash_area_erased_val(fap), sizeof(wbs_aligned));
        memcpy(wbs_aligned, img_chunk + img_chunk_len, rem_bytes);

        rc = flash_area_write(fap, curr_off + img_chunk_len, wbs_aligned,
                              flash_area_align(fap));
    }

    if (rc == 0) {
        curr_off += img_chunk_len + rem_bytes;
        if (curr_off == img_size) {
#ifdef MCUBOOT_ERASE_PROGRESSIVELY
            /* Assure that sector for image trailer was erased. */
            /* Check whether it was erased during previous upload. */
            off_t start = flash_sector_get_off(&status_sector);

            if (erase_range(fap, start, start) < 0) {
                rc = MGMT_ERR_EUNKNOWN;
                goto out;
            }
#endif
            rc = BOOT_HOOK_CALL(boot_serial_uploaded_hook, 0, img_num, fap,
                                img_size);
            if (rc) {
                BOOT_LOG_ERR("Error %d post upload hook", rc);
                goto out;
            }
        }
    } else {
    out_invalid_data:
        rc = MGMT_ERR_EINVAL;
    }

out:
    BOOT_LOG_INF("RX: 0x%x", rc);
    zcbor_map_start_encode(cbor_state, 10);
    zcbor_tstr_put_lit_cast(cbor_state, "rc");
    zcbor_uint32_put(cbor_state, rc);
    if (rc == 0) {
        zcbor_tstr_put_lit_cast(cbor_state, "off");
        zcbor_uint32_put(cbor_state, curr_off);
    }
    zcbor_map_end_encode(cbor_state, 10);

    boot_serial_output();
    flash_area_close(fap);

#ifdef MCUBOOT_ENC_IMAGES
    if (curr_off == img_size) {
        /* Last sector received, now start a decryption on the image if it is encrypted*/
        rc = boot_handle_enc_fw();
    }
#endif //#ifdef MCUBOOT_ENC_IMAGES
}

/*
 * Send rc code only.
 */
static void
bs_rc_rsp(int rc_code)
{
    zcbor_map_start_encode(cbor_state, 10);
    zcbor_tstr_put_lit_cast(cbor_state, "rc");
    zcbor_uint32_put(cbor_state, rc_code);
    zcbor_map_end_encode(cbor_state, 10);
    boot_serial_output();
}


#ifdef MCUBOOT_BOOT_MGMT_ECHO
static void
bs_echo(char *buf, int len)
{
    struct Echo echo = { 0 };
    size_t decoded_len;
    uint32_t rc = MGMT_ERR_EINVAL;
    uint_fast8_t result = cbor_decode_Echo((const uint8_t *)buf, len, &echo, &decoded_len);

    if ((result != ZCBOR_SUCCESS) || (len != decoded_len)) {
        goto out;
    }

    if (echo._Echo_d.value == NULL) {
        goto out;
    }

    zcbor_map_start_encode(cbor_state, 10);
    zcbor_tstr_put_term(cbor_state, "r");
    if (zcbor_tstr_encode(cbor_state, &echo._Echo_d) && zcbor_map_end_encode(cbor_state, 10)) {
        boot_serial_output();
        return;
    } else {
        rc = MGMT_ERR_ENOMEM;
    }

out:
    reset_cbor_state();
    bs_rc_rsp(rc);
}
#endif

/*
 * Reset, and (presumably) boot to newly uploaded image. Flush console
 * before restarting.
 */
static void
bs_reset(char *buf, int len)
{
    bs_rc_rsp(0);

#ifdef __ZEPHYR__
#ifdef CONFIG_MULTITHREADING
    k_sleep(K_MSEC(250));
#else
    k_busy_wait(250000);
#endif
    sys_reboot(SYS_REBOOT_COLD);
#elif __ESPRESSIF__
    esp_rom_delay_us(250000);
    bootloader_reset();
#else
    os_cputime_delay_usecs(250000);
    hal_system_reset();
#endif
}

/*
 * Parse incoming line of input from console.
 * Expect newtmgr protocol with serial transport.
 */
void
boot_serial_input(char *buf, int len)
{
    struct nmgr_hdr *hdr;

    hdr = (struct nmgr_hdr *)buf;
    if (len < sizeof(*hdr) ||
      (hdr->nh_op != NMGR_OP_READ && hdr->nh_op != NMGR_OP_WRITE) ||
      (ntohs(hdr->nh_len) < len - sizeof(*hdr))) {
        return;
    }
    bs_hdr = hdr;
    hdr->nh_group = ntohs(hdr->nh_group);

    buf += sizeof(*hdr);
    len -= sizeof(*hdr);

    reset_cbor_state();

    /*
     * Limited support for commands.
     */
    if (hdr->nh_group == MGMT_GROUP_ID_IMAGE) {
        switch (hdr->nh_id) {
        case IMGMGR_NMGR_ID_STATE:
            bs_list(buf, len);
            break;
        case IMGMGR_NMGR_ID_UPLOAD:
            bs_upload(buf, len);
            break;
        default:
            bs_rc_rsp(MGMT_ERR_ENOTSUP);
            break;
        }
    } else if (hdr->nh_group == MGMT_GROUP_ID_DEFAULT) {
        switch (hdr->nh_id) {
        case NMGR_ID_ECHO:
#ifdef MCUBOOT_BOOT_MGMT_ECHO
            bs_echo(buf, len);
#endif
            break;
        case NMGR_ID_CONS_ECHO_CTRL:
            bs_rc_rsp(0);
            break;
        case NMGR_ID_RESET:
            bs_reset(buf, len);
            break;
        default:
            bs_rc_rsp(MGMT_ERR_ENOTSUP);
            break;
        }
    } else if (MCUBOOT_PERUSER_MGMT_GROUP_ENABLED == 1) {
        if (bs_peruser_system_specific(hdr, buf, len, cbor_state) == 0) {
            boot_serial_output();
        }
    } else {
        bs_rc_rsp(MGMT_ERR_ENOTSUP);
    }
#ifdef MCUBOOT_SERIAL_WAIT_FOR_DFU
    bs_entry = true;
#endif
}

static void
boot_serial_output(void)
{
    char *data;
    int len;
    uint16_t crc;
    uint16_t totlen;
    char pkt_start[2] = { SHELL_NLIP_PKT_START1, SHELL_NLIP_PKT_START2 };
    char buf[BOOT_SERIAL_OUT_MAX + sizeof(*bs_hdr) + sizeof(crc) + sizeof(totlen)];
    char encoded_buf[BASE64_ENCODE_SIZE(sizeof(buf))];

    data = bs_obuf;
    len = (uint32_t)cbor_state->payload_mut - (uint32_t)bs_obuf;

    bs_hdr->nh_op++;
    bs_hdr->nh_flags = 0;
    bs_hdr->nh_len = htons(len);
    bs_hdr->nh_group = htons(bs_hdr->nh_group);

#ifdef __ZEPHYR__
    crc =  crc16_itu_t(CRC16_INITIAL_CRC, (uint8_t *)bs_hdr, sizeof(*bs_hdr));
    crc =  crc16_itu_t(crc, data, len);
#elif __ESPRESSIF__
    /* For ESP32 it was used the CRC API in rom/crc.h */
    crc =  ~crc16_be(~CRC16_INITIAL_CRC, (uint8_t *)bs_hdr, sizeof(*bs_hdr));
    crc =  ~crc16_be(~crc, (uint8_t *)data, len);
#else
    crc = crc16_ccitt(CRC16_INITIAL_CRC, bs_hdr, sizeof(*bs_hdr));
    crc = crc16_ccitt(crc, data, len);
#endif
    crc = htons(crc);

    boot_uf->write(pkt_start, sizeof(pkt_start));

    totlen = len + sizeof(*bs_hdr) + sizeof(crc);
    totlen = htons(totlen);

    memcpy(buf, &totlen, sizeof(totlen));
    totlen = sizeof(totlen);
    memcpy(&buf[totlen], bs_hdr, sizeof(*bs_hdr));
    totlen += sizeof(*bs_hdr);
    memcpy(&buf[totlen], data, len);
    totlen += len;
    memcpy(&buf[totlen], &crc, sizeof(crc));
    totlen += sizeof(crc);
#ifdef __ZEPHYR__
    size_t enc_len;
    base64_encode(encoded_buf, sizeof(encoded_buf), &enc_len, buf, totlen);
    totlen = enc_len;
#elif __ESPRESSIF__
    size_t enc_len;
    base64_encode((unsigned char *)encoded_buf, sizeof(encoded_buf), &enc_len, (unsigned char *)buf, totlen);
    totlen = enc_len;
#else
    totlen = base64_encode(buf, totlen, encoded_buf, 1);
#endif
    boot_uf->write(encoded_buf, totlen);
    boot_uf->write("\n", 1);
    BOOT_LOG_INF("TX");
}

/*
 * Returns 1 if full packet has been received.
 */
static int
boot_serial_in_dec(char *in, int inlen, char *out, int *out_off, int maxout)
{
    int rc;
    uint16_t crc;
    uint16_t len;

#ifdef __ZEPHYR__
    int err;
    err = base64_decode( &out[*out_off], maxout - *out_off, &rc, in, inlen - 2);
    if (err) {
        return -1;
    }
#elif __ESPRESSIF__
    int err;
    err = base64_decode((unsigned char *)&out[*out_off], maxout - *out_off, (size_t *)&rc, (unsigned char *)in, inlen);
    if (err) {
        return -1;
    }
#else
    if (*out_off + base64_decode_len(in) >= maxout) {
        return -1;
    }
    rc = base64_decode(in, &out[*out_off]);
    if (rc < 0) {
        return -1;
    }
#endif

    *out_off += rc;
    if (*out_off <= sizeof(uint16_t)) {
        return 0;
    }

    len = ntohs(*(uint16_t *)out);
    if (len != *out_off - sizeof(uint16_t)) {
        return 0;
    }

    if (len > *out_off - sizeof(uint16_t)) {
        len = *out_off - sizeof(uint16_t);
    }

    out += sizeof(uint16_t);
#ifdef __ZEPHYR__
    crc = crc16_itu_t(CRC16_INITIAL_CRC, out, len);
#elif __ESPRESSIF__
    crc = ~crc16_be(~CRC16_INITIAL_CRC, (uint8_t *)out, len);
#else
    crc = crc16_ccitt(CRC16_INITIAL_CRC, out, len);
#endif
    if (crc || len <= sizeof(crc)) {
        return 0;
    }
    *out_off -= sizeof(crc);
    out[*out_off] = '\0';

    return 1;
}

/*
 * Task which waits reading console, expecting to get image over
 * serial port.
 */
static void
boot_serial_read_console(const struct boot_uart_funcs *f,int timeout_in_ms)
{
    int rc;
    int off;
    int dec_off = 0;
    int full_line;
    int max_input;
    int elapsed_in_ms = 0;

    boot_uf = f;
    max_input = sizeof(in_buf);

    off = 0;
    while (timeout_in_ms > 0 || bs_entry) {
        /*
         * Don't enter CPU idle state here if timeout based serial recovery is
         * used as otherwise the boot process hangs forever, waiting for input
         * from serial console (if single-thread mode is used).
         */
#ifndef MCUBOOT_SERIAL_WAIT_FOR_DFU
        MCUBOOT_CPU_IDLE();
#endif
        MCUBOOT_WATCHDOG_FEED();
#ifdef MCUBOOT_SERIAL_WAIT_FOR_DFU
        uint32_t start = k_uptime_get_32();
#endif
        rc = f->read(in_buf + off, sizeof(in_buf) - off, &full_line);
        if (rc <= 0 && !full_line) {
            goto check_timeout;
        }
        off += rc;
        if (!full_line) {
            if (off == max_input) {
                /*
                 * Full line, no newline yet. Reset the input buffer.
                 */
                off = 0;
            }
            goto check_timeout;
        }
        if (in_buf[0] == SHELL_NLIP_PKT_START1 &&
          in_buf[1] == SHELL_NLIP_PKT_START2) {
            dec_off = 0;
            rc = boot_serial_in_dec(&in_buf[2], off - 2, dec_buf, &dec_off, max_input);
        } else if (in_buf[0] == SHELL_NLIP_DATA_START1 &&
          in_buf[1] == SHELL_NLIP_DATA_START2) {
            rc = boot_serial_in_dec(&in_buf[2], off - 2, dec_buf, &dec_off, max_input);
        }

        /* serve errors: out of decode memory, or bad encoding */
        if (rc == 1) {
            boot_serial_input(&dec_buf[2], dec_off - 2);
        }
        off = 0;
check_timeout:
        /* Subtract elapsed time */
#ifdef MCUBOOT_SERIAL_WAIT_FOR_DFU
        elapsed_in_ms = (k_uptime_get_32() - start);
#endif
        timeout_in_ms -= elapsed_in_ms;
    }
}

/*
 * Task which waits reading console, expecting to get image over
 * serial port.
 */
void
boot_serial_start(const struct boot_uart_funcs *f)
{
    bs_entry = true;
    boot_serial_read_console(f,0);
}

#ifdef MCUBOOT_SERIAL_WAIT_FOR_DFU
/*
 * Task which waits reading console for a certain amount of timeout.
 * If within this timeout no mcumgr command is received, the function is
 * returning, else the serial boot is never exited
 */
void
boot_serial_check_start(const struct boot_uart_funcs *f, int timeout_in_ms)
{
    bs_entry = false;
    boot_serial_read_console(f,timeout_in_ms);
}
#endif
