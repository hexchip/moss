/*
 * Copyright (c) 2023 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "indicator.h"

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

static const struct gpio_dt_spec rgb_r = GPIO_DT_SPEC_GET(DT_ALIAS(led_rgb_red), gpios);
static const struct gpio_dt_spec rgb_g = GPIO_DT_SPEC_GET(DT_ALIAS(led_rgb_green), gpios);
static const struct gpio_dt_spec rgb_b = GPIO_DT_SPEC_GET(DT_ALIAS(led_rgb_blue), gpios);

#define INDICATOR_THREAD_STACK_SIZE KB(4)
#define INDICATOR_THREAD_PRIORITY   10

static void indicator_task(void *, void *, void *);
K_THREAD_DEFINE(indicator_thread_id, INDICATOR_THREAD_STACK_SIZE, indicator_task, NULL, NULL, NULL,
		INDICATOR_THREAD_PRIORITY, 0, 0);

enum indicator_op {
	INDICATOR_SET,
	INDICATOR_BLINK,
};

struct indicator_msg {
	enum indicator_op op;
	int r;
	int g;
	int b;
};

K_MSGQ_DEFINE(indicator_msgq, sizeof(struct indicator_msg), 10, 4);

static void indicator_set(int r, int g, int b)
{
	gpio_pin_set_dt(&rgb_r, r);
	gpio_pin_set_dt(&rgb_g, g);
	gpio_pin_set_dt(&rgb_b, b);
}

static void indicator_task(void *, void *, void *)
{
	struct indicator_msg msg;
	int cur_r = 0, cur_g = 0, cur_b = 0;

	while (1) {
		k_msgq_get(&indicator_msgq, &msg, K_FOREVER);

		switch (msg.op) {
		case INDICATOR_SET:
			indicator_set(msg.r, msg.g, msg.b);
			cur_r = msg.r;
			cur_g = msg.g;
			cur_b = msg.b;
			break;
		case INDICATOR_BLINK:
			for (int i = 0; i < 3; i++) {
				indicator_set(msg.r, msg.g, msg.b);
				k_msleep(100);
				indicator_set(0, 0, 0);
				k_msleep(100);
			}
			indicator_set(cur_r, cur_g, cur_b);
			break;
		default:
			break;
		}
	}
}

static bool last_has_face = false;

void indicator_set_has_face(bool has_face)
{
	if (last_has_face == has_face) {
		return;
	}

	if (has_face) {
		struct indicator_msg msg = {INDICATOR_SET, 1, 1, 0};
		k_msgq_put(&indicator_msgq, &msg, K_NO_WAIT);
	} else {
		struct indicator_msg msg = {INDICATOR_SET, 0, 0, 0};
		k_msgq_put(&indicator_msgq, &msg, K_NO_WAIT);
	}

	last_has_face = has_face;
}

void indicator_set_result(bool pass)
{
	if (pass) {
		struct indicator_msg msg = {INDICATOR_SET, 0, 1, 0};
		k_msgq_put(&indicator_msgq, &msg, K_NO_WAIT);
	} else {
		struct indicator_msg msg = {INDICATOR_SET, 1, 0, 0};
		k_msgq_put(&indicator_msgq, &msg, K_NO_WAIT);
	}
}

void indicator_show_registered(bool succeed)
{
	if (succeed) {
		struct indicator_msg msg = {INDICATOR_BLINK, 0, 0, 1};
		k_msgq_put(&indicator_msgq, &msg, K_NO_WAIT);
	} else {
		struct indicator_msg msg = {INDICATOR_BLINK, 1, 0, 0};
		k_msgq_put(&indicator_msgq, &msg, K_NO_WAIT);
	}
}

static int indicator_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&rgb_r)) {
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&rgb_g)) {
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&rgb_b)) {
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&rgb_r, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return ret;
	}

	ret = gpio_pin_configure_dt(&rgb_g, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return ret;
	}

	ret = gpio_pin_configure_dt(&rgb_b, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return ret;
	}

	indicator_set(0, 0, 0);

	return 0;
}

SYS_INIT(indicator_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
