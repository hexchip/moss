/*
 * Copyright (c) 2023 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

#include <lvgl.h>

#include "screen.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display, CONFIG_APP_LOG_LEVEL);

#define DISPLAY_THREAD_STACK_SIZE KB(8)
#define DISPLAY_THREAD_PRIORITY   7

static const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

K_THREAD_STACK_DEFINE(display_work_stack, DISPLAY_THREAD_STACK_SIZE);
static struct k_work_q display_work_q;

static void display_tick_cb(struct k_work *work)
{
	ARG_UNUSED(work);

	lv_task_handler();
}

K_WORK_DEFINE(display_tick_work, display_tick_cb);

static void display_timer_cb(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	k_work_submit_to_queue(&display_work_q, &display_tick_work);
}

K_TIMER_DEFINE(display_timer, display_timer_cb, NULL);

int display_init(void)
{
	if (!device_is_ready(display)) {
		LOG_ERR("Display not ready: %s", display->name);
		return -ENODEV;
	}

	LOG_DBG("Display %s ready", display->name);

	lv_disp_t *disp = lv_disp_get_default();
	LOG_DBG("Display resolution: %dx%d", disp->driver->hor_res, disp->driver->ver_res);

	k_work_queue_start(&display_work_q, display_work_stack,
			   K_THREAD_STACK_SIZEOF(display_work_stack), DISPLAY_THREAD_PRIORITY,
			   NULL);

	lv_obj_t *screen = screen_create();
	lv_scr_load(screen);

	display_blanking_off(display);
	k_timer_start(&display_timer, K_MSEC(CONFIG_LV_DISP_DEF_REFR_PERIOD),
		      K_MSEC(CONFIG_LV_DISP_DEF_REFR_PERIOD));

	return 0;
}
