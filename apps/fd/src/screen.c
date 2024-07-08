/*
 * Copyright (c) 2023 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "screen.h"

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(screen, CONFIG_APP_LOG_LEVEL);

#define DISPLAY_NODE   DT_CHOSEN(zephyr_display)
#define DISPLAY_WIDTH  DT_PROP(DISPLAY_NODE, width)
#define DISPLAY_HEIGHT DT_PROP(DISPLAY_NODE, height)

#define IMAGE_WIDTH  320
#define IMAGE_HEIGHT 240

#define SPAN(n) (5 * n)

#define COLOR_RED    (lv_color_hex(0xFF0000))
#define COLOR_GREEN  (lv_color_hex(0x00FF00))
#define COLOR_YELLOW (lv_color_hex(0xFFCC00))
#define COLOR_WHITE  (lv_color_hex(0x999999))

static const int IMAGE_X_OFFSET = -(IMAGE_WIDTH - DISPLAY_WIDTH) / 2;
static const int IMAGE_Y_OFFSET = -(IMAGE_HEIGHT - DISPLAY_HEIGHT) / 2;

static lv_obj_t *preview;
static lv_obj_t *face_rect;
static lv_obj_t *detect_status;
static lv_obj_t *verify_status;
static lv_obj_t *register_status;

static lv_img_dsc_t preview_img;
static Z_GENERIC_DOT_SECTION(psram_section) uint16_t preview_buf[IMAGE_WIDTH * IMAGE_HEIGHT];

#if defined(CONFIG_DISPLAY_DEBUG_SHOW_FPS)
static lv_obj_t *fps;
static uint32_t frame_cnt = 0;

static void fps_tick_cb(struct k_work *work);
K_WORK_DEFINE(fps_tick_work, fps_tick_cb);

static void fps_timer_cb(struct k_timer *timer);
K_TIMER_DEFINE(fps_timer, fps_timer_cb, NULL);
#endif /* CONFIG_DISPLAY_DEBUG_SHOW_FPS */

lv_obj_t *screen_create(void)
{
	lv_obj_t *screen = lv_obj_create(NULL);
	lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

	preview = lv_img_create(screen);
	lv_obj_set_pos(preview, IMAGE_X_OFFSET, IMAGE_Y_OFFSET);
	lv_obj_set_size(preview, IMAGE_WIDTH, IMAGE_HEIGHT); 

// 	lv_obj_t *guide = lv_label_create(screen);
// 	lv_obj_set_style_text_color(guide, COLOR_RED, LV_PART_MAIN);
// 	lv_obj_set_size(guide, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
// 	lv_obj_align(guide, LV_ALIGN_BOTTOM_MID, 0, SPAN(-1));
// 	lv_label_set_text_static(guide, "K1: REGISTER | K2: RECOGNIZE"
// #if defined(CONFIG_FD_ANTI_SPOOFING)
// 					" | K3: IR"
// #endif /* CONFIG_FD_ANTI_SPOOFING */
// 	);

	face_rect = lv_obj_create(screen);
	lv_obj_set_style_bg_opa(face_rect, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(face_rect, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(face_rect, COLOR_YELLOW, LV_PART_MAIN);
	lv_obj_set_style_radius(face_rect, 0, LV_PART_MAIN);
	lv_obj_add_flag(face_rect, LV_OBJ_FLAG_HIDDEN);

	detect_status = lv_label_create(screen);
	lv_obj_set_style_text_color(detect_status, COLOR_WHITE, LV_PART_MAIN);
	lv_obj_set_size(detect_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_align(detect_status, LV_ALIGN_TOP_LEFT, SPAN(1), SPAN(1));
	lv_label_set_text(detect_status, "has_face: 0, anti_sp: 0");

	verify_status = lv_label_create(screen);
	lv_obj_set_style_text_color(verify_status, COLOR_YELLOW, LV_PART_MAIN);
	lv_obj_set_size(verify_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_align(verify_status, LV_ALIGN_BOTTOM_LEFT, SPAN(1), SPAN(-4));
	lv_label_set_text(verify_status, "----");

	register_status = lv_label_create(screen);
	lv_obj_set_style_text_color(register_status, COLOR_YELLOW, LV_PART_MAIN);
	lv_obj_set_size(register_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_align(register_status, LV_ALIGN_BOTTOM_LEFT, SPAN(1), SPAN(-1));
	lv_label_set_text(register_status, "registered: 0");

#if defined(CONFIG_DISPLAY_DEBUG_SHOW_FPS)
	fps = lv_label_create(screen);
	lv_obj_set_style_text_color(fps, COLOR_WHITE, LV_PART_MAIN);
	lv_obj_set_size(fps, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_align(fps, LV_ALIGN_TOP_RIGHT, SPAN(-1), SPAN(1));
	lv_label_set_text(fps, "0 FPS");

	k_timer_start(&fps_timer, K_SECONDS(1), K_SECONDS(1));
#endif /* CONFIG_DISPLAY_DEBUG_SHOW_FPS */

	preview_img.header.cf = LV_IMG_CF_TRUE_COLOR;
	preview_img.header.w = IMAGE_WIDTH;
	preview_img.header.h = IMAGE_HEIGHT;
	preview_img.data = (uint8_t *)preview_buf;
	preview_img.data_size = sizeof(preview_buf);

	return screen;
}

void screen_update_info(const fd_info_t *info)
{
	if (info->frame.width != IMAGE_WIDTH * 2 || info->frame.height != IMAGE_HEIGHT * 2) {
		return;
	}

	/* VYUY to RGB565 and 50% point resize */
	const uint8_t *src = (const uint8_t *)info->frame.data;
	for (uint32_t y = 0; y < IMAGE_HEIGHT; y++) {
		const uint32_t src_y = y * 2;
		for (uint32_t x = 0; x < IMAGE_WIDTH; x++) {
			const uint32_t src_x = x * 2;
			const uint32_t src_idx = src_y * info->frame.width + src_x;

			const uint8_t v0 = src[src_idx * 2 + 0];
			const uint8_t y0 = src[src_idx * 2 + 1];
			const uint8_t u1 = src[src_idx * 2 + 2];
			/* y1 is not used */

			int r = y0 + 1.403f * (v0 - 128);
			int g = y0 - 0.344f * (u1 - 128) - 0.714f * (v0 - 128);
			int b = y0 + 1.770f * (u1 - 128);

			r = r < 0 ? 0 : r > 255 ? 255 : r;
			g = g < 0 ? 0 : g > 255 ? 255 : g;
			b = b < 0 ? 0 : b > 255 ? 255 : b;

			const uint16_t rgb565 = ((r >> 3) & BIT_MASK(5)) << 11 |
						((g >> 2) & BIT_MASK(6)) << 5 |
						((b >> 3) & BIT_MASK(5));

			preview_buf[y * IMAGE_WIDTH + x] = rgb565;
		}
	}

	lv_img_set_src(preview, &preview_img);

	lv_label_set_text_fmt(detect_status, "has_face: %d, anti_sp: %d",
			      info->face != NULL && info->face->head_pose_status ? 1 : 0,
			      info->face != NULL ? info->face->anti_spoofing_status : 0);

	if (info->face != NULL) {
		lv_obj_set_pos(face_rect, (lv_coord_t)(info->face->detect_rect.x / 2 + IMAGE_X_OFFSET),
			       (lv_coord_t)(info->face->detect_rect.y / 2 + IMAGE_Y_OFFSET));
		lv_obj_set_size(face_rect, (lv_coord_t)(info->face->detect_rect.w / 2),
				(lv_coord_t)(info->face->detect_rect.h / 2));
		lv_obj_clear_flag(face_rect, LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_add_flag(face_rect, LV_OBJ_FLAG_HIDDEN);
		lv_label_set_text(verify_status, "----");
		lv_obj_set_style_text_color(verify_status, COLOR_YELLOW, LV_PART_MAIN);
		lv_obj_set_style_text_color(register_status, COLOR_YELLOW, LV_PART_MAIN);
		lv_obj_set_style_border_color(face_rect, COLOR_YELLOW, LV_PART_MAIN);
	}

#if defined(CONFIG_DISPLAY_DEBUG_SHOW_FPS)
	frame_cnt++;
#endif /* CONFIG_DISPLAY_DEBUG_SHOW_FPS */
}

void screen_update_registered(uint32_t count)
{
	lv_label_set_text_fmt(register_status, "registered: %d", count);
}

void screen_update_compare(uint32_t index, float score, bool pass)
{
	lv_label_set_text_fmt(verify_status, "id: %d, score: %.2f", index, score);
	lv_obj_set_style_text_color(verify_status, pass ? COLOR_GREEN : COLOR_RED, LV_PART_MAIN);
	lv_obj_set_style_text_color(register_status, pass ? COLOR_GREEN : COLOR_RED, LV_PART_MAIN);
	lv_obj_set_style_border_color(face_rect, pass ? COLOR_GREEN : COLOR_RED, LV_PART_MAIN);
}

#if defined(CONFIG_DISPLAY_DEBUG_SHOW_FPS)
static void fps_tick_cb(struct k_work *work)
{
	ARG_UNUSED(work);

	lv_label_set_text_fmt(fps, "%d FPS", frame_cnt);
	frame_cnt = 0;
}

static void fps_timer_cb(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	k_work_submit(&fps_tick_work);
}
#endif /* CONFIG_DISPLAY_DEBUG_SHOW_FPS */
