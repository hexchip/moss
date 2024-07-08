/*
 * Copyright (c) 2023 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <lvgl.h>

#include "gcs_fd_service.h"

lv_obj_t *screen_create(void);

void screen_update_info(const fd_info_t *info);

void screen_update_registered(uint32_t count);

void screen_update_compare(uint32_t index, float score, bool pass);
