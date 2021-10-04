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

#ifndef __BOOTUTIL_SERIAL_PRIV_H__
#define __BOOTUTIL_SERIAL_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * From shell.h
 */
#define SHELL_NLIP_PKT_START1   6
#define SHELL_NLIP_PKT_START2   9

#define SHELL_NLIP_DATA_START1  4
#define SHELL_NLIP_DATA_START2  20

/*
 * From newtmgr.h
 */
#define MGMT_ERR_OK             0
#define MGMT_ERR_EUNKNOWN       2
#define MGMT_ERR_EINVAL         3
#define MGMT_ERR_ENOTSUP        8

#define NMGR_OP_READ            0
#define NMGR_OP_WRITE           2

#define MGMT_GROUP_ID_DEFAULT   0
#define MGMT_GROUP_ID_IMAGE     1
#define MGMT_GROUP_ID_PERUSER  64

#define NMGR_ID_CONS_ECHO_CTRL  1
#define NMGR_ID_RESET           5

struct nmgr_hdr {
    uint8_t  nh_op;             /* NMGR_OP_XXX */
    uint8_t  nh_flags;
    uint16_t nh_len;            /* length of the payload */
    uint16_t nh_group;          /* NMGR_GROUP_XXX */
    uint8_t  nh_seq;            /* sequence number */
    uint8_t  nh_id;             /* message ID within group */
};

/*
 * From imgmgr.h
 */
#define IMGMGR_NMGR_ID_STATE            0
#define IMGMGR_NMGR_ID_UPLOAD           1

void boot_serial_input(char *buf, int len);
extern const struct boot_uart_funcs *boot_uf;

/**
 * @brief Selects direct image to upload according to the "image"
 * parameter of the mcumgr update frame.
 *
 * @param[in] image_id  the value of the "image" parameter of the
 *                      mcumgr update frame to be translated.
 *
 * @return flash area ID for the image if defined;
 *         -EINVAL when flash area for given image number has not been
 *         defined.
 */
extern int flash_area_id_from_direct_image(int image_id);

#ifdef __cplusplus
}
#endif

#endif /*  __BOOTUTIL_SERIAL_PRIV_H__ */
