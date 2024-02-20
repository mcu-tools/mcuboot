/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2024 Elektroline Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef H_COPY_PRIV_
#define H_COPY_PRIV_

#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_COPY_WITH_REVERT

/**
 *  This function gets image type (update, recover) from its header.
 *  This is to let the bootloader know whether he should update from
 *  secondary or tertiary slot.
 */
int copy_get_slot_type(struct boot_loader_state *state);

#endif /* MCUBOOT_COPY_WITH_REVERT */
#endif /* H_COPY_PRIV_ */
