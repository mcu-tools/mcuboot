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

#include <bootutil/sign_key.h>
#include <bootutil/enc_key.h>
unsigned char enc_key[] = {
  0x96, 0x69, 0xd2, 0xcf, 0x0e, 0xb1, 0xc6, 0x56, 0xf2, 0xa0, 0x1f, 0x46,
  0x06, 0xd3, 0x49, 0x31,
};
static unsigned int enc_key_len = 16;
const struct bootutil_key bootutil_enc_key = {
    .key = enc_key,
    .len = &enc_key_len,
};
