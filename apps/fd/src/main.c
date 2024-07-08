/*
 * Copyright (c) 2023 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/device.h>
#include <zephyr/drivers/video.h>

#include "boot_cp.h"
#include "ic_message.h"
#include "resource_load.h"
#include "comp_fd.h"
#if defined(CONFIG_DISPLAY_DEBUG)
#include "display.h"
#endif /* CONFIG_DISPLAY_DEBUG */

#define CP_FLASH_BASE_ADDRESS        (0x68000000)
#define FLASH_CP_IMAGE_REGION_OFFSET DT_REG_ADDR(DT_NODE_BY_FIXED_PARTITION_LABEL(cp_code))
#define CP_BOOT_ADDRESS              (CP_FLASH_BASE_ADDRESS + FLASH_CP_IMAGE_REGION_OFFSET)

static const struct device *video = DEVICE_DT_GET(DT_NODELABEL(dvp));

static uint16_t last_key = 0;

static void input_work_cb(struct k_work *work)
{
	ARG_UNUSED(work);

	switch (last_key) {
	case INPUT_KEY_1:
		comp_fd_capture();
		break;
	case INPUT_KEY_2:
		comp_fd_compare();
		break;
	default:
		break;
	}
}

K_WORK_DEFINE(input_work, input_work_cb);

static void input_cb(struct input_event *evt)
{
	if (evt->type == INPUT_EV_KEY && evt->value) {
		last_key = evt->code;
		k_work_submit(&input_work);
	}
}

INPUT_LISTENER_CB_DEFINE(NULL, input_cb);

extern int webusb_gcl_init(void);

int main(void)
{
	int enable_vertical_flip = 1;
	video_set_ctrl(video, VIDEO_CID_VFLIP, (void*) &enable_vertical_flip);

	boot_cp((const void *)CP_BOOT_ADDRESS);

#if defined(CONFIG_GCL_WEBUSB)
	webusb_gcl_init();
#endif /* CONFIG_GCL_WEBUSB */

	ic_message_init();

#if defined(CONFIG_GCL_COMP_RES)
	components_resource_load();
#endif /* CONFIG_GCL_COMP_RES */

#if defined(CONFIG_DISPLAY_DEBUG)
	display_init();
#endif /* CONFIG_DISPLAY_DEBUG */

	comp_fd_init();

	return 0;
}
