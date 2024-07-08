/*
 * Copyright (c) 2023 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "comp_fd.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/crc.h>

#include "indicator.h"
#if defined(CONFIG_DISPLAY_DEBUG)
#include "screen.h"
#endif /* CONFIG_DISPLAY_DEBUG */
#include "gcs_fd_service.h"

#define FD_UART_BUFFER_SIZE 512

/* change this to any other UART peripheral if desired */
#define FD_UART_DEVICE_NODE DT_NODELABEL(uart2)

static const struct device *const fd_uart_dev = DEVICE_DT_GET(FD_UART_DEVICE_NODE);

static fd_feature_t feature_store[FEATURES_MAX_COUNT];

static void append_crc_to_string_hex(char* str, size_t max_len, uint16_t crc)
{
    size_t len = strlen(str);
    if (len + sizeof(uint16_t) < max_len) { // 确保有足够的空间
		sprintf(str + len, "%04X\n", crc); // 将CRC格式化为十六进制字符串并附加
    }
}

static void print_uart(char *buf) 
{
	int msg_len = strlen(buf);

	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(fd_uart_dev, buf[i]);
	}
}

static void sendMessage(const char *format, ...) 
{
	static char buffer[FD_UART_BUFFER_SIZE];

	va_list args;
	va_start(args, format);
	vsnprintf(buffer, FD_UART_BUFFER_SIZE, format, args);
 	va_end(args);

	printk("%s\n", buffer);

	uint16_t crc = crc16_itu_t(0x0000, buffer, strlen(buffer));

	append_crc_to_string_hex(buffer, FD_UART_BUFFER_SIZE, crc);

	print_uart(buffer);
}

static void comp_fd_info_callback(const fd_info_t *info)
{
	indicator_set_has_face(info->face != NULL && info->face->head_pose_status
#if defined(CONFIG_FD_ANTI_SPOOFING)
			       && info->face->anti_spoofing_status
#endif /* CONFIG_FD_ANTI_SPOOFING */
	);

#if defined(CONFIG_DISPLAY_DEBUG)
	screen_update_info(info);
#endif /* CONFIG_DISPLAY_DEBUG */

	if (info->face != NULL) {
		sendMessage(
				"{\"type\":\"face-info\","
				"\"face\":{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d},"
				"\"head\":{\"yaw\":%f,\"pitch\":%f,\"roll\":%f,\"pose\":%d},"
				"\"anti_spoofing\":{\"score\":%f,\"status\":%d}}\n",
				info->face->detect_rect.x, info->face->detect_rect.y,
				info->face->detect_rect.w, info->face->detect_rect.h, info->face->head_yaw,
				info->face->head_pitch, info->face->head_roll, info->face->head_pose_status,
				info->face->anti_spoofing_score, info->face->anti_spoofing_status);
	}
}

int comp_fd_init(void)
{
	/* 初始化人脸识别服务 */
	fd_service_gcs_init();

	/* 获取当前参数、修改并设置 */
	fd_service_gcs_params_t params;
	fd_service_gcs_params_get(&params);
	params.fd_face_detect_thres = 0.4f;
	params.fd_face_align_yawthres = 30.0f;
	params.fd_face_align_pitchthres = 30.0f;
	params.fd_face_align_rollthres = 30.0f;
#if defined(CONFIG_FD_ANTI_SPOOFING)
	params.fd_anti_spoofing_thres = 0.7f;
#else  /* CONFIG_FD_ANTI_SPOOFING */
	params.fd_anti_spoofing_thres = 0.0f;
#endif /* CONFIG_FD_ANTI_SPOOFING */
	fd_service_gcs_params_set(&params);

#if defined(CONFIG_WEBUSB_DEBUG)
	/* 开启 PC 调试工具 */
	fd_service_gcs_work_mode_set(FD_GCS_WORK_MODE_DEBUG, NULL);
#endif /* CONFIG_WEBUSB_DEBUG */

#if defined(CONFIG_FD_ANTI_SPOOFING)
	/* 使能灰度模式 (活体检测必须) */
	fd_service_gcs_gray_mode_set(true);
#endif /* CONFIG_FD_ANTI_SPOOFING */

	/* 注册人脸信息回调 */
	fd_service_gcs_callback(comp_fd_info_callback);

	/* 加载人脸特征库
	 * 注意：实际应用中通常是从 Flash 加载。此处仅以一个全局数组作为示例。
	 */
	// fd_service_gcs_features_load(feature_store, ARRAY_SIZE(feature_store));

	/* 启动服务 */
	fd_service_gcs_start();

	return 0;
}

int comp_fd_capture(void)
{
	int ret;
	bool succeed;
	uint32_t count;

	/* 将当前人脸特征抓取下来 */
	ret = fd_service_gcs_feature_capture();
	printk("feature capture ret=%d\n", ret);
	succeed = ret == 0;

	/*
	 * 导出特征库
	 * 注意：实际应用中通常会将特征库写入到 Flash 中。此处仅作为示例。
	 */
	ret = fd_service_gcs_features_dump(feature_store, &count);
	printk("captured feature count: %d\n", count);

	indicator_show_registered(succeed);

#if defined(CONFIG_DISPLAY_DEBUG)
	screen_update_registered(count);
#endif /* CONFIG_DISPLAY_DEBUG */

	sendMessage(
			"{"
			"\"type\":\"face-capture\","
			"\"is_succeed\":%s,",
			"\"feature_count\":%d"
			"}",
			succeed ? "true" : "false",
			count);

	return 0;
}

int comp_fd_compare(void)
{
	uint32_t index;
	float score;
	int ret;

	/* 对比特征库中的人脸，并输出最符合的 id 和分值 */
	ret = fd_service_gcs_feature_compare(&index, &score);
	if (ret) {
		printk("compare failed: %d\n", ret);
		return ret;
	}
	printk("index: %d, score: %f\n", index, score);

	/* 此处示例将分值大于 0.8 认为是同一个人，实际应用中需要根据实际情况调整 */
	bool pass = score > 0.8f;

	indicator_set_result(pass);

#if defined(CONFIG_DISPLAY_DEBUG)
	screen_update_compare(index, score, pass);
#endif /* CONFIG_DISPLAY_DEBUG */

	sendMessage(
			"{"
			"\"type\":\"face-compare\","
			"\"is_succeed\":%s,"
			"\"feature_id\":%d,"
			"\"score\":%f"
			"}",
			ret == 0 ? "true" : "false",
			index,
			score);

	return 0;
}
