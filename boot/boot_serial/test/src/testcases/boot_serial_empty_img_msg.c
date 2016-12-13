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
#include "boot_test.h"

TEST_CASE(boot_serial_empty_img_msg)
{
    char buf[sizeof(struct nmgr_hdr) + 32];
    struct nmgr_hdr *hdr;

    hdr = (struct nmgr_hdr *)buf;
    memset(hdr, 0, sizeof(*hdr));
    hdr->nh_op = NMGR_OP_WRITE;
    hdr->nh_group = htons(MGMT_GROUP_ID_IMAGE);
    hdr->nh_id = IMGMGR_NMGR_OP_UPLOAD;
    hdr->nh_len = htons(2);
    strcpy((char *)(hdr + 1), "{}");

    tx_msg(buf, sizeof(*hdr) + 2);
}
