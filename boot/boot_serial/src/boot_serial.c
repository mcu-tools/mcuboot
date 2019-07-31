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

#include "sysflash/sysflash.h"

#include "bootutil/bootutil_log.h"

#ifdef __ZEPHYR__
#include <misc/reboot.h>
#include <misc/byteorder.h>
#include <misc/__assert.h>
#include <flash.h>
#include <crc.h>
#include <base64.h>
#include <cbor.h>
#else
#include <bsp/bsp.h>
#include <hal/hal_system.h>
#include <os/endian.h>
#include <os/os_cputime.h>
#include <crc/crc16.h>
#include <base64/base64.h>
#include <tinycbor/cbor.h>
#include <tinycbor/cbor_buf_reader.h>
#endif /* __ZEPHYR__ */

#include <flash_map_backend/flash_map_backend.h>
#include <hal/hal_flash.h>
#include <os/os.h>
#include <os/os_malloc.h>

#include <bootutil/image.h>

#include "boot_serial/boot_serial.h"
#include "boot_serial_priv.h"

#ifdef CONFIG_BOOT_ERASE_PROGRESSIVELY
#include "bootutil_priv.h"
#endif

MCUBOOT_LOG_MODULE_DECLARE(mcuboot);

#define BOOT_SERIAL_INPUT_MAX   512
#define BOOT_SERIAL_OUT_MAX	80

#ifdef __ZEPHYR__
/* base64 lib encodes data to null-terminated string */
#define BASE64_ENCODE_SIZE(in_size) ((((((in_size) - 1) / 3) * 4) + 4) + 1)

#define CRC16_INITIAL_CRC       0       /* what to seed crc16 with */
#define CRC_CITT_POLYMINAL 0x1021

#define ntohs(x) sys_be16_to_cpu(x)
#define htons(x) sys_cpu_to_be16(x)
#endif

static char in_buf[BOOT_SERIAL_INPUT_MAX + 1];
static char dec_buf[BOOT_SERIAL_INPUT_MAX + 1];
const struct boot_uart_funcs *boot_uf;
static uint32_t curr_off;
static uint32_t img_size;
static struct nmgr_hdr *bs_hdr;

static char bs_obuf[BOOT_SERIAL_OUT_MAX];

static int bs_cbor_writer(struct cbor_encoder_writer *, const char *data,
  int len);
static void boot_serial_output(void);

static struct cbor_encoder_writer bs_writer = {
    .write = bs_cbor_writer
};
static CborEncoder bs_root;
static CborEncoder bs_rsp;

int
bs_cbor_writer(struct cbor_encoder_writer *cew, const char *data, int len)
{
    if (cew->bytes_written + len > sizeof(bs_obuf)) {
        return CborErrorOutOfMemory;
    }

    memcpy(&bs_obuf[cew->bytes_written], data, len);
    cew->bytes_written += len;

    return 0;
}

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

/*
 * List images.
 */
static void
bs_list(char *buf, int len)
{
    CborEncoder images;
    CborEncoder image;
    struct image_header hdr;
    uint8_t tmpbuf[64];
    int i, area_id;
    const struct flash_area *fap;

    cbor_encoder_create_map(&bs_root, &bs_rsp, CborIndefiniteLength);
    cbor_encode_text_stringz(&bs_rsp, "images");
    cbor_encoder_create_array(&bs_rsp, &images, CborIndefiniteLength);
    for (i = 0; i < 2; i++) {
        area_id = flash_area_id_from_image_slot(i);
        if (flash_area_open(area_id, &fap)) {
            continue;
        }

        flash_area_read(fap, 0, &hdr, sizeof(hdr));

        if (hdr.ih_magic != IMAGE_MAGIC ||
          bootutil_img_validate(0, &hdr, fap, tmpbuf, sizeof(tmpbuf),
                                NULL, 0, NULL)) {
            flash_area_close(fap);
            continue;
        }
        flash_area_close(fap);

        cbor_encoder_create_map(&images, &image, CborIndefiniteLength);
        cbor_encode_text_stringz(&image, "slot");
        cbor_encode_int(&image, i);
        cbor_encode_text_stringz(&image, "version");

        bs_list_img_ver((char *)tmpbuf, sizeof(tmpbuf), &hdr.ih_ver);
        cbor_encode_text_stringz(&image, (char *)tmpbuf);
        cbor_encoder_close_container(&images, &image);
    }
    cbor_encoder_close_container(&bs_rsp, &images);
    cbor_encoder_close_container(&bs_root, &bs_rsp);
    boot_serial_output();
}

/*
 * Image upload request.
 */
static void
bs_upload(char *buf, int len)
{
    CborParser parser;
    struct cbor_buf_reader reader;
    struct CborValue root_value;
    struct CborValue value;
    uint8_t img_data[512];
    long long int off = UINT_MAX;
    size_t img_blen = 0;
    uint8_t rem_bytes;
    long long int data_len = UINT_MAX;
    size_t slen;
    char name_str[8];
    const struct flash_area *fap = NULL;
    int rc;
#ifdef CONFIG_BOOT_ERASE_PROGRESSIVELY
    static off_t off_last = -1;
    struct flash_sector sector;
#endif

    memset(img_data, 0, sizeof(img_data));

    /*
     * Expected data format.
     * {
     *   "data":<img_data>
     *   "len":<image len>
     *   "off":<current offset of image data>
     * }
     */

    /*
     * Object comes within { ... }
     */
    cbor_buf_reader_init(&reader, (uint8_t *)buf, len);
#ifdef __ZEPHYR__
    cbor_parser_cust_reader_init(&reader.r, 0, &parser, &root_value);
#else
    cbor_parser_init(&reader.r, 0, &parser, &root_value);
#endif
    if (!cbor_value_is_container(&root_value)) {
        goto out_invalid_data;
    }
    if (cbor_value_enter_container(&root_value, &value)) {
        goto out_invalid_data;
    }
    while (cbor_value_is_valid(&value)) {
        /*
         * Decode key.
         */
        if (cbor_value_calculate_string_length(&value, &slen)) {
            goto out_invalid_data;
        }
        if (!cbor_value_is_text_string(&value) ||
            slen >= sizeof(name_str) - 1) {
            goto out_invalid_data;
        }
        if (cbor_value_copy_text_string(&value, name_str, &slen, &value)) {
            goto out_invalid_data;
        }
        name_str[slen] = '\0';
        if (!strcmp(name_str, "data")) {
            /*
             * Image data
             */
            if (value.type != CborByteStringType) {
                goto out_invalid_data;
            }
            if (cbor_value_calculate_string_length(&value, &slen) ||
                slen >= sizeof(img_data)) {
                goto out_invalid_data;
            }
            if (cbor_value_copy_byte_string(&value, img_data, &slen, &value)) {
                goto out_invalid_data;
            }
            img_blen = slen;
        } else if (!strcmp(name_str, "off")) {
            /*
             * Offset of the data.
             */
            if (value.type != CborIntegerType) {
                goto out_invalid_data;
            }
            if (cbor_value_get_int64(&value, &off)) {
                goto out_invalid_data;
            }
            if (cbor_value_advance(&value)) {
                goto out_invalid_data;
            }
        } else if (!strcmp(name_str, "len")) {
            /*
             * Length of the image. This should only be present in the first
             * block of data; when offset is 0.
             */
            if (value.type != CborIntegerType) {
                goto out_invalid_data;
            }
            if (cbor_value_get_int64(&value, &data_len)) {
                goto out_invalid_data;
            }
            if (cbor_value_advance(&value)) {
                goto out_invalid_data;
            }
        } else {
            /*
             * Unknown keys.
             */
            if (cbor_value_advance(&value)) {
                goto out_invalid_data;
            }
        }
    }
    if (off == UINT_MAX) {
        /*
         * Offset must be set in every block.
         */
        goto out_invalid_data;
    }

    rc = flash_area_open(flash_area_id_from_image_slot(0), &fap);
    if (rc) {
        rc = MGMT_ERR_EINVAL;
        goto out;
    }

    if (off == 0) {
        curr_off = 0;
        if (data_len > fap->fa_size) {
            goto out_invalid_data;
        }
#ifndef CONFIG_BOOT_ERASE_PROGRESSIVELY
        rc = flash_area_erase(fap, 0, fap->fa_size);
        if (rc) {
            goto out_invalid_data;
        }
#endif
        img_size = data_len;
    }
    if (off != curr_off) {
        rc = 0;
        goto out;
    }
    if (curr_off + img_blen < img_size) {
        rem_bytes = img_blen % flash_area_align(fap);
        if (rem_bytes) {
            img_blen -= rem_bytes;
        }
    }

#ifdef CONFIG_BOOT_ERASE_PROGRESSIVELY
    rc = flash_area_sector_from_off(curr_off + img_blen, &sector);
    if (rc) {
        BOOT_LOG_ERR("Unable to determine flash sector size");
        goto out;
    }
    if (off_last != sector.fs_off) {
        off_last = sector.fs_off;
        BOOT_LOG_INF("Erasing sector at offset 0x%x", sector.fs_off);
        rc = flash_area_erase(fap, sector.fs_off, sector.fs_size);
        if (rc) {
            BOOT_LOG_ERR("Error %d while erasing sector", rc);
            goto out;
        }
    }
#endif

    BOOT_LOG_INF("Writing at 0x%x until 0x%x", curr_off, curr_off + img_blen);
    rc = flash_area_write(fap, curr_off, img_data, img_blen);
    if (rc == 0) {
        curr_off += img_blen;
#ifdef CONFIG_BOOT_ERASE_PROGRESSIVELY
        if (curr_off == img_size) {
            /* get the last sector offset */
            rc = flash_area_sector_from_off(boot_status_off(fap), &sector);
            if (rc) {
                BOOT_LOG_ERR("Unable to determine flash sector of"
                             "the image trailer");
                goto out;
            }
            /* Assure that sector for image trailer was erased. */
            /* Check whether it was erased during previous upload. */
            if (off_last < sector.fs_off) {
                BOOT_LOG_INF("Erasing sector at offset 0x%x", sector.fs_off);
                rc = flash_area_erase(fap, sector.fs_off, sector.fs_size);
                if (rc) {
                    BOOT_LOG_ERR("Error %d while erasing sector", rc);
                    goto out;
                }
            }
        }
#endif
    } else {
    out_invalid_data:
        rc = MGMT_ERR_EINVAL;
    }

out:
    BOOT_LOG_INF("RX: 0x%x", rc);
    cbor_encoder_create_map(&bs_root, &bs_rsp, CborIndefiniteLength);
    cbor_encode_text_stringz(&bs_rsp, "rc");
    cbor_encode_int(&bs_rsp, rc);
    if (rc == 0) {
        cbor_encode_text_stringz(&bs_rsp, "off");
        cbor_encode_uint(&bs_rsp, curr_off);
    }
    cbor_encoder_close_container(&bs_root, &bs_rsp);

    boot_serial_output();
    flash_area_close(fap);
}

/*
 * Console echo control/image erase. Send empty response, don't do anything.
 */
static void
bs_empty_rsp(char *buf, int len)
{
    cbor_encoder_create_map(&bs_root, &bs_rsp, CborIndefiniteLength);
    cbor_encode_text_stringz(&bs_rsp, "rc");
    cbor_encode_int(&bs_rsp, 0);
    cbor_encoder_close_container(&bs_root, &bs_rsp);
    boot_serial_output();
}

/*
 * Reset, and (presumably) boot to newly uploaded image. Flush console
 * before restarting.
 */
static void
bs_reset(char *buf, int len)
{
    bs_empty_rsp(buf, len);

#ifdef __ZEPHYR__
    k_sleep(250);
    sys_reboot(SYS_REBOOT_COLD);
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

    bs_writer.bytes_written = 0;
#ifdef __ZEPHYR__
    cbor_encoder_cust_writer_init(&bs_root, &bs_writer, 0);
#else
    cbor_encoder_init(&bs_root, &bs_writer, 0);
#endif

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
            bs_empty_rsp(buf, len);
            break;
        }
    } else if (hdr->nh_group == MGMT_GROUP_ID_DEFAULT) {
        switch (hdr->nh_id) {
        case NMGR_ID_CONS_ECHO_CTRL:
            bs_empty_rsp(buf, len);
            break;
        case NMGR_ID_RESET:
            bs_reset(buf, len);
            break;
        default:
            break;
        }
    }
}

static void
boot_serial_output(void)
{
    char *data;
    int len;
    uint16_t crc;
    uint16_t totlen;
    char pkt_start[2] = { SHELL_NLIP_PKT_START1, SHELL_NLIP_PKT_START2 };
    char buf[BOOT_SERIAL_OUT_MAX];
    char encoded_buf[BASE64_ENCODE_SIZE(BOOT_SERIAL_OUT_MAX)];

    data = bs_obuf;
    len = bs_writer.bytes_written;

    bs_hdr->nh_op++;
    bs_hdr->nh_flags = 0;
    bs_hdr->nh_len = htons(len);
    bs_hdr->nh_group = htons(bs_hdr->nh_group);

#ifdef __ZEPHYR__
    crc =  crc16((u8_t *)bs_hdr, sizeof(*bs_hdr), CRC_CITT_POLYMINAL,
                 CRC16_INITIAL_CRC, false);
    crc =  crc16(data, len, CRC_CITT_POLYMINAL, crc, true);
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
#else
    totlen = base64_encode(buf, totlen, encoded_buf, 1);
#endif
    boot_uf->write(encoded_buf, totlen);
    boot_uf->write("\n\r", 2);
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

    if (*out_off > sizeof(uint16_t)) {
        len = ntohs(*(uint16_t *)out);
        if (len != *out_off - sizeof(uint16_t)) {
            return 0;
        }

        if (len > *out_off - sizeof(uint16_t)) {
            len = *out_off - sizeof(uint16_t);
        }

        out += sizeof(uint16_t);
#ifdef __ZEPHYR__
        crc = crc16(out, len, CRC_CITT_POLYMINAL, CRC16_INITIAL_CRC, true);
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
    return 0;
}

/*
 * Task which waits reading console, expecting to get image over
 * serial port.
 */
void
boot_serial_start(const struct boot_uart_funcs *f)
{
    int rc;
    int off;
    int dec_off;
    int full_line;
    int max_input;

    boot_uf = f;
    max_input = sizeof(in_buf);

    off = 0;
    while (1) {
        rc = f->read(in_buf + off, sizeof(in_buf) - off, &full_line);
        if (rc <= 0 && !full_line) {
            continue;
        }
        off += rc;
        if (!full_line) {
            if (off == max_input) {
                /*
                 * Full line, no newline yet. Reset the input buffer.
                 */
                off = 0;
            }
            continue;
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
    }
}
