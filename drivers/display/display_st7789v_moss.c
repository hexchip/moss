/*
 * Copyright (c) 2017 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 * Copyright (c) 2019 Nordic Semiconductor ASA
 * Copyright (c) 2019 Marc Reilly
 * Copyright (c) 2019 PHYTEC Messtechnik GmbH
 * Copyright (c) 2020 Endian Technologies AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sitronix_st7789v

#include "display_st7789v_moss.h"

#include <display_csk6_lcd.h>

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/display.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_st7789v, 4);

#include "csk6_cm33/include/spi.h"

struct st7789v_config {
	struct spi_dt_spec bus;
	struct gpio_dt_spec cmd_data_gpio;
	struct gpio_dt_spec reset_gpio;
	uint8_t vcom;
	uint8_t gctrl;
	bool vdv_vrh_enable;
	uint8_t vrh_value;
	uint8_t vdv_value;
	uint8_t mdac;
	uint8_t gamma;
	uint8_t colmod;
	uint8_t lcm;
	uint8_t porch_param[5];
	uint8_t cmd2en_param[4];
	uint8_t pwctrl1_param[2];
	uint8_t pvgam_param[14];
	uint8_t nvgam_param[14];
	uint8_t ram_param[2];
	uint8_t rgb_param[3];
	uint16_t height;
	uint16_t width;
};

struct st7789v_display_send_ctx {
	const uint8_t *buffer;
	uint32_t h;
	uint32_t w;

	uint32_t line_size;
	uint32_t line_cnt;
	uint32_t curr_cnt;
	uint32_t step_offset;
	uint8_t rotate_90;
};

struct st7789v_data {
	uint16_t x_offset;
	uint16_t y_offset;
	void *spi;
};

#ifdef CONFIG_ST7789V_RGB565
#define ST7789V_PIXEL_SIZE 2u
#else
#define ST7789V_PIXEL_SIZE 3u
#endif

static void st7789v_set_lcd_margins(const struct device *dev,
				    uint16_t x_offset, uint16_t y_offset)
{
	struct st7789v_data *data = dev->data;

	data->x_offset = x_offset;
	data->y_offset = y_offset;
}

__attribute__((section(".itcm"))) static int32_t st7789v_transmit(const struct device *dev, uint8_t cmd,
			     uint8_t *tx_data, size_t tx_len)
{
	const struct st7789v_config *config = dev->config;
	int r;
	struct st7789v_data *data = dev->data;

	gpio_pin_set_dt(&config->cmd_data_gpio, 1);

	r = display_csk6_lcd_cmd_datas_send(data->spi, &cmd, 1);
	if (r != 0) {
		return r;
	}

	if ((tx_data != NULL) && (tx_len > 0)) {
		gpio_pin_set_dt(&config->cmd_data_gpio, 0);
		r = display_csk6_lcd_cmd_datas_send(data->spi, tx_data, tx_len);
		if (r != 0) {
			LOG_ERR("SPI_Send failed, error code:%d", r);
			return r;
		}
	}

	return 0;
}

static void st7789v_exit_sleep(const struct device *dev)
{
	st7789v_transmit(dev, ST7789V_CMD_SLEEP_OUT, NULL, 0);
	k_sleep(K_MSEC(120));
}

static void st7789v_reset_display(const struct device *dev)
{
	LOG_DBG("Resetting display");

	const struct st7789v_config *config = dev->config;
	if (config->reset_gpio.port != NULL) {
		k_sleep(K_MSEC(1));
		gpio_pin_set_dt(&config->reset_gpio, 1);
		k_sleep(K_MSEC(6));
		gpio_pin_set_dt(&config->reset_gpio, 0);
		k_sleep(K_MSEC(20));
	} else {
		st7789v_transmit(dev, ST7789V_CMD_SW_RESET, NULL, 0);
		k_sleep(K_MSEC(5));
	}
}

static int st7789v_blanking_on(const struct device *dev)
{
	st7789v_transmit(dev, ST7789V_CMD_DISP_OFF, NULL, 0);
	return 0;
}

static int st7789v_blanking_off(const struct device *dev)
{
	st7789v_transmit(dev, ST7789V_CMD_DISP_ON, NULL, 0);
	return 0;
}

static int st7789v_read(const struct device *dev,
			const uint16_t x,
			const uint16_t y,
			const struct display_buffer_descriptor *desc,
			void *buf)
{
	return -ENOTSUP;
}

static void st7789v_set_mem_area(const struct device *dev, const uint16_t x,
				 const uint16_t y, const uint16_t w, const uint16_t h)
{
	struct st7789v_data *data = dev->data;
	uint16_t spi_data[2];

	uint16_t ram_x = x + data->x_offset;
	uint16_t ram_y = y + data->y_offset;

	spi_data[0] = sys_cpu_to_be16(ram_x);
	spi_data[1] = sys_cpu_to_be16(ram_x + w - 1);
	st7789v_transmit(dev, ST7789V_CMD_CASET, (uint8_t *)&spi_data[0], 4);

	spi_data[0] = sys_cpu_to_be16(ram_y);
	spi_data[1] = sys_cpu_to_be16(ram_y + h - 1);
	st7789v_transmit(dev, ST7789V_CMD_RASET, (uint8_t *)&spi_data[0], 4);
}

__attribute__((section(".itcm"))) static int
st7789v_write(const struct device *dev, const uint16_t x, const uint16_t y,
	      const struct display_buffer_descriptor *desc, const void *buf)
{
	const struct st7789v_config *config = dev->config;
	struct st7789v_data *data = dev->data;
	uint32_t new_h = desc->height;
	uint32_t new_w = desc->width;
	uint32_t new_x = x;
	uint32_t new_y = y;

#if CONFIG_ST7789V_MOSS_ROTATE_90
	new_h = desc->width;
	new_w = desc->height;
	new_y = x;
	new_x = config->width - (y + desc->height);
#endif

	st7789v_set_mem_area(dev, new_x, new_y, new_w, new_h);
	st7789v_transmit(dev, ST7789V_CMD_RAMWR, NULL, 0);
	gpio_pin_set_dt(&config->cmd_data_gpio, 0);

	int r = display_csk6_lcd_display_datas_send(data->spi, buf, desc->height, desc->width,
						    IS_ENABLED(CONFIG_ST7789V_MOSS_ROTATE_90));
	if (r) {
		LOG_ERR("display_csk6_lcd_display_datas_send failed, r:%d", r);
	}

	return r;
}

static void *st7789v_get_framebuffer(const struct device *dev)
{
	return NULL;
}

static int st7789v_set_brightness(const struct device *dev,
			   const uint8_t brightness)
{
	return -ENOTSUP;
}

static int st7789v_set_contrast(const struct device *dev,
			 const uint8_t contrast)
{
	return -ENOTSUP;
}

static void st7789v_get_capabilities(const struct device *dev,
			      struct display_capabilities *capabilities)
{
	const struct st7789v_config *config = dev->config;

	memset(capabilities, 0, sizeof(struct display_capabilities));
	capabilities->x_resolution = config->width;
	capabilities->y_resolution = config->height;

#ifdef CONFIG_ST7789V_RGB565
	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
	capabilities->current_pixel_format = PIXEL_FORMAT_RGB_565;
#else
	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_888;
	capabilities->current_pixel_format = PIXEL_FORMAT_RGB_888;
#endif
	capabilities->current_orientation = DISPLAY_ORIENTATION_NORMAL;
}

static int st7789v_set_pixel_format(const struct device *dev,
			     const enum display_pixel_format pixel_format)
{
#ifdef CONFIG_ST7789V_RGB565
	if (pixel_format == PIXEL_FORMAT_RGB_565) {
#else
	if (pixel_format == PIXEL_FORMAT_RGB_888) {
#endif
		return 0;
	}
	LOG_ERR("Pixel format change not implemented");
	return -ENOTSUP;
}

static int st7789v_set_orientation(const struct device *dev,
			    const enum display_orientation orientation)
{
	if (orientation == DISPLAY_ORIENTATION_NORMAL) {
		return 0;
	}
	LOG_ERR("Changing display orientation not implemented");
	return -ENOTSUP;
}

static void st7789v_lcd_init(const struct device *dev)
{
	struct st7789v_data *data = dev->data;
	const struct st7789v_config *config = dev->config;
	uint8_t tmp;

	st7789v_set_lcd_margins(dev, data->x_offset,
				data->y_offset);

	st7789v_transmit(dev, ST7789V_CMD_CMD2EN,
			 (uint8_t *)config->cmd2en_param,
			 sizeof(config->cmd2en_param));

	st7789v_transmit(dev, ST7789V_CMD_PORCTRL,
			 (uint8_t *)config->porch_param,
			 sizeof(config->porch_param));

	/* Digital Gamma Enable, default disabled */
	tmp = 0x00;
	st7789v_transmit(dev, ST7789V_CMD_DGMEN, &tmp, 1);

	/* Frame Rate Control in Normal Mode, default value */
	tmp = 0x0f;
	st7789v_transmit(dev, ST7789V_CMD_FRCTRL2, &tmp, 1);

	tmp = config->gctrl;
	st7789v_transmit(dev, ST7789V_CMD_GCTRL, &tmp, 1);

	tmp = config->vcom;
	st7789v_transmit(dev, ST7789V_CMD_VCOMS, &tmp, 1);

	if (config->vdv_vrh_enable) {
		tmp = 0x01;
		st7789v_transmit(dev, ST7789V_CMD_VDVVRHEN, &tmp, 1);

		tmp = config->vrh_value;
		st7789v_transmit(dev, ST7789V_CMD_VRH, &tmp, 1);

		tmp = config->vdv_value;
		st7789v_transmit(dev, ST7789V_CMD_VDS, &tmp, 1);
	}

	st7789v_transmit(dev, ST7789V_CMD_PWCTRL1,
			 (uint8_t *)config->pwctrl1_param,
			 sizeof(config->pwctrl1_param));

	/* Memory Data Access Control */
	tmp = config->mdac;
	st7789v_transmit(dev, ST7789V_CMD_MADCTL, &tmp, 1);

	/* Interface Pixel Format */
	tmp = config->colmod;
	st7789v_transmit(dev, ST7789V_CMD_COLMOD, &tmp, 1);

	tmp = config->lcm;
	st7789v_transmit(dev, ST7789V_CMD_LCMCTRL, &tmp, 1);

	tmp = config->gamma;
	st7789v_transmit(dev, ST7789V_CMD_GAMSET, &tmp, 1);

	st7789v_transmit(dev, ST7789V_CMD_INV_OFF, NULL, 0);

	st7789v_transmit(dev, ST7789V_CMD_PVGAMCTRL,
			 (uint8_t *)config->pvgam_param,
			 sizeof(config->pvgam_param));

	st7789v_transmit(dev, ST7789V_CMD_NVGAMCTRL,
			 (uint8_t *)config->nvgam_param,
			 sizeof(config->nvgam_param));

	st7789v_transmit(dev, ST7789V_CMD_RAMCTRL,
			 (uint8_t *)config->ram_param,
			 sizeof(config->ram_param));

	st7789v_transmit(dev, ST7789V_CMD_RGBCTRL,
			 (uint8_t *)config->rgb_param,
			 sizeof(config->rgb_param));
}

static int st7789v_init(const struct device *dev)
{
	const struct st7789v_config *config = dev->config;
	struct st7789v_data *data = dev->data;

	LOG_INF("st7789v_init");
	if (!device_is_ready(config->bus.bus)) {
		LOG_ERR("SPI device not ready");
		return -ENODEV;
	} else {

		data->spi = display_csk6_lcd_init(config->bus.bus->name,
						  CONFIG_ST7789V_MOSS_DMA_CHANNEL);
		if (data->spi == NULL) {
			LOG_ERR("hal spi device is null");
			k_panic();
		}
	}

	if (config->reset_gpio.port != NULL) {
		if (!device_is_ready(config->reset_gpio.port)) {
			LOG_ERR("Reset GPIO device not ready");
			return -ENODEV;
		}

		if (gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT_INACTIVE)) {
			LOG_ERR("Couldn't configure reset pin");
			return -EIO;
		}
	}

	if (!device_is_ready(config->cmd_data_gpio.port)) {
		LOG_ERR("Reset GPIO device not ready");
		return -ENODEV;
	}

	if (gpio_pin_configure_dt(&config->cmd_data_gpio, GPIO_OUTPUT)) {
		LOG_ERR("Couldn't configure cmd/DATA pin");
		return -EIO;
	}

	st7789v_reset_display(dev);

	st7789v_blanking_on(dev);

	st7789v_lcd_init(dev);

	st7789v_exit_sleep(dev);
	LOG_INF("st7789v_init done");
	return 0;
}

#ifdef CONFIG_PM_DEVICE
static int st7789v_pm_action(const struct device *dev,
			     enum pm_device_action action)
{
	int ret = 0;

	switch (action) {
	case PM_DEVICE_ACTION_RESUME:
		st7789v_exit_sleep(dev);
		break;
	case PM_DEVICE_ACTION_SUSPEND:
		st7789v_transmit(dev, ST7789V_CMD_SLEEP_IN, NULL, 0);
		break;
	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}
#endif /* CONFIG_PM_DEVICE */

static const struct display_driver_api st7789v_api = {
	.blanking_on = st7789v_blanking_on,
	.blanking_off = st7789v_blanking_off,
	.write = st7789v_write,
	.read = st7789v_read,
	.get_framebuffer = st7789v_get_framebuffer,
	.set_brightness = st7789v_set_brightness,
	.set_contrast = st7789v_set_contrast,
	.get_capabilities = st7789v_get_capabilities,
	.set_pixel_format = st7789v_set_pixel_format,
	.set_orientation = st7789v_set_orientation,
};

#define ST7789V_INIT(inst)							\
	static const struct st7789v_config st7789v_config_ ## inst = {		\
		.bus = SPI_DT_SPEC_INST_GET(inst, SPI_OP_MODE_MASTER |          \
							  SPI_WORD_SET(8), 0),	\
		.cmd_data_gpio = GPIO_DT_SPEC_INST_GET(inst, cmd_data_gpios),	\
		.reset_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, reset_gpios, {}),	\
		.vcom = DT_INST_PROP(inst, vcom),				\
		.gctrl = DT_INST_PROP(inst, gctrl),				\
		.vdv_vrh_enable = (DT_INST_NODE_HAS_PROP(inst, vrhs)            \
					&& DT_INST_NODE_HAS_PROP(inst, vdvs)),	\
		.vrh_value = DT_INST_PROP_OR(inst, vrhs, 0),			\
		.vdv_value = DT_INST_PROP_OR(inst, vdvs, 0),			\
		.mdac = DT_INST_PROP(inst, mdac),				\
		.gamma = DT_INST_PROP(inst, gamma),				\
		.colmod = DT_INST_PROP(inst, colmod),				\
		.lcm = DT_INST_PROP(inst, lcm),					\
		.porch_param = DT_INST_PROP(inst, porch_param),			\
		.cmd2en_param = DT_INST_PROP(inst, cmd2en_param),		\
		.pwctrl1_param = DT_INST_PROP(inst, pwctrl1_param),		\
		.pvgam_param = DT_INST_PROP(inst, pvgam_param),			\
		.nvgam_param = DT_INST_PROP(inst, nvgam_param),			\
		.ram_param = DT_INST_PROP(inst, ram_param),			\
		.rgb_param = DT_INST_PROP(inst, rgb_param),			\
		.width = DT_INST_PROP(inst, width),				\
		.height = DT_INST_PROP(inst, height),				\
	};									\
										\
	static struct st7789v_data st7789v_data_ ## inst = {			\
		.x_offset = DT_INST_PROP(inst, x_offset),			\
		.y_offset = DT_INST_PROP(inst, y_offset),			\
	};									\
										\
	PM_DEVICE_DT_INST_DEFINE(inst, st7789v_pm_action);			\
										\
	DEVICE_DT_INST_DEFINE(inst, &st7789v_init, PM_DEVICE_DT_INST_GET(inst), \
			&st7789v_data_ ## inst, &st7789v_config_ ## inst,	\
			POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY,		\
			&st7789v_api);

DT_INST_FOREACH_STATUS_OKAY(ST7789V_INIT)
