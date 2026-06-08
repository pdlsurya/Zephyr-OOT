/*
 * Copyright (c) 2026 Surya Poudel
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT surya_tft_spi

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr_oot/drivers/tft_drv.h>

LOG_MODULE_REGISTER(tft_drv);

#define TFT_TX_PIXELS_PER_CHUNK 512U
#define TFT_TX_BUF_SIZE (TFT_TX_PIXELS_PER_CHUNK * 2U)

#define TFT_CMD_SWRESET 0x01U
#define TFT_CMD_SLPOUT 0x11U
#define TFT_CMD_INVOFF 0x20U
#define TFT_CMD_INVON 0x21U
#define TFT_CMD_DISPON 0x29U
#define TFT_CMD_CASET 0x2AU
#define TFT_CMD_RASET 0x2BU
#define TFT_CMD_RAMWR 0x2CU
#define TFT_CMD_VSCRDEF 0x33U
#define TFT_CMD_MADCTL 0x36U
#define TFT_CMD_VSCSAD 0x37U
#define TFT_CMD_COLMOD 0x3AU
#define TFT_CMD_WRDISBV 0x51U
#define TFT_CMD_WRCTRLD 0x53U

#define TFT_MAD_MY 0x80U
#define TFT_MAD_MX 0x40U
#define TFT_MAD_MV 0x20U
#define TFT_MAD_BGR 0x08U

enum tft_controller {
	TFT_CTRL_ILI9341 = 0U,
	TFT_CTRL_ST7789,
	TFT_CTRL_ST7796,
};

struct tft_drv_config {
	struct spi_dt_spec bus;
	struct gpio_dt_spec dc_gpio;
	struct gpio_dt_spec reset_gpio;
	uint16_t native_width;
	uint16_t native_height;
	uint8_t rotation;
	enum tft_controller controller;
	bool has_reset_gpio;
};

struct tft_drv_data {
	struct k_mutex lock;
	uint16_t width;
	uint16_t height;
	uint8_t rotation;
	bool stream_window_set;
	uint32_t stream_pixels_remaining;
	uint8_t tx_buf[TFT_TX_BUF_SIZE];
};

static int tft_spi_write(const struct device *dev, const uint8_t *buf,
			 size_t len)
{
	const struct tft_drv_config *config = dev->config;
	struct spi_buf tx_buf = {
		.buf = (void *)buf,
		.len = len,
	};
	struct spi_buf_set tx_bufs = {
		.buffers = &tx_buf,
		.count = 1,
	};

	return spi_write_dt(&config->bus, &tx_bufs);
}

static void tft_release_bus(const struct device *dev)
{
	const struct tft_drv_config *config = dev->config;

	(void)spi_release_dt(&config->bus);
}

static int tft_write_command_locked(const struct device *dev, uint8_t command)
{
	const struct tft_drv_config *config = dev->config;
	int ret;

	ret = gpio_pin_set_dt(&config->dc_gpio, 0);
	if (ret != 0) {
		return ret;
	}

	return tft_spi_write(dev, &command, 1U);
}

static int tft_write_data_locked(const struct device *dev, const uint8_t *data,
				 size_t len)
{
	const struct tft_drv_config *config = dev->config;
	int ret;

	ret = gpio_pin_set_dt(&config->dc_gpio, 1);
	if (ret != 0) {
		return ret;
	}

	return tft_spi_write(dev, data, len);
}

static int tft_send_command_locked(const struct device *dev, uint8_t command,
				   const uint8_t *data, size_t len)
{
	int ret;

	ret = tft_write_command_locked(dev, command);
	if ((ret == 0) && (data != NULL) && (len > 0U)) {
		ret = tft_write_data_locked(dev, data, len);
	}

	tft_release_bus(dev);
	return ret;
}

static uint8_t tft_get_madctl(enum tft_controller controller, uint8_t rotation)
{
	switch (controller) {
	case TFT_CTRL_ST7796:
		switch (rotation) {
		case TFT_ROTATION_0:
			return TFT_MAD_MX | TFT_MAD_BGR;
		case TFT_ROTATION_90:
			return TFT_MAD_MV | TFT_MAD_BGR;
		case TFT_ROTATION_180:
			return TFT_MAD_MY | TFT_MAD_BGR;
		case TFT_ROTATION_270:
			return TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_MV |
			       TFT_MAD_BGR;
		default:
			return TFT_MAD_MX | TFT_MAD_BGR;
		}
	case TFT_CTRL_ST7789:
		switch (rotation) {
		case TFT_ROTATION_0:
			return TFT_MAD_BGR;
		case TFT_ROTATION_90:
			return TFT_MAD_MV | TFT_MAD_MX | TFT_MAD_BGR;
		case TFT_ROTATION_180:
			return TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_BGR;
		case TFT_ROTATION_270:
			return TFT_MAD_MV | TFT_MAD_MY | TFT_MAD_BGR;
		default:
			return TFT_MAD_BGR;
		}
	case TFT_CTRL_ILI9341:
	default:
		switch (rotation) {
		case TFT_ROTATION_0:
			return 0x00U;
		case TFT_ROTATION_90:
			return 0x61U;
		case TFT_ROTATION_180:
			return 0xC0U;
		case TFT_ROTATION_270:
			return 0xA0U;
		default:
			return 0x00U;
		}
	}
}

static bool tft_controller_inverted(enum tft_controller controller)
{
	return controller != TFT_CTRL_ILI9341;
}

static void tft_update_dimensions(const struct tft_drv_config *config,
				  struct tft_drv_data *data, uint8_t rotation)
{
	if ((rotation == TFT_ROTATION_90) || (rotation == TFT_ROTATION_270)) {
		data->width = config->native_height;
		data->height = config->native_width;
	} else {
		data->width = config->native_width;
		data->height = config->native_height;
	}

	data->rotation = rotation;
}

static int tft_set_address_window_locked(const struct device *dev, uint16_t x0,
					 uint16_t y0, uint16_t x1, uint16_t y1)
{
	uint8_t col_data[4] = {
		(uint8_t)(x0 >> 8),
		(uint8_t)(x0 & 0xFFU),
		(uint8_t)(x1 >> 8),
		(uint8_t)(x1 & 0xFFU),
	};
	uint8_t row_data[4] = {
		(uint8_t)(y0 >> 8),
		(uint8_t)(y0 & 0xFFU),
		(uint8_t)(y1 >> 8),
		(uint8_t)(y1 & 0xFFU),
	};
	int ret;

	ret = tft_send_command_locked(dev, TFT_CMD_CASET, col_data,
				      sizeof(col_data));
	if (ret != 0) {
		return ret;
	}

	ret = tft_send_command_locked(dev, TFT_CMD_RASET, row_data,
				      sizeof(row_data));
	if (ret != 0) {
		return ret;
	}

	return tft_write_command_locked(dev, TFT_CMD_RAMWR);
}

static int tft_set_rotation_locked(const struct device *dev, uint8_t rotation)
{
	const struct tft_drv_config *config = dev->config;
	struct tft_drv_data *data = dev->data;
	uint8_t madctl;
	int ret;

	if (rotation > TFT_ROTATION_270) {
		return -EINVAL;
	}

	madctl = tft_get_madctl(config->controller, rotation);
	ret = tft_send_command_locked(dev, TFT_CMD_MADCTL, &madctl, 1U);
	if (ret == 0) {
		tft_update_dimensions(config, data, rotation);
	}

	return ret;
}

static void tft_pack_colors(uint8_t *dst, const uint16_t *src, size_t len)
{
	size_t i;

	for (i = 0U; i < len; i++) {
		uint16_t color = src[i];

		dst[(i * 2U)] = (uint8_t)(color >> 8);
		dst[(i * 2U) + 1U] = (uint8_t)(color & 0xFFU);
	}
}

static int tft_set_address_window_impl(const struct device *dev, uint16_t x0,
				       uint16_t y0, uint16_t x1, uint16_t y1)
{
	struct tft_drv_data *data = dev->data;
	uint32_t width;
	uint32_t height;
	int ret;

	if ((x0 > x1) || (y0 > y1)) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	if ((x1 >= data->width) || (y1 >= data->height)) {
		ret = -EINVAL;
		goto out;
	}

	width = (uint32_t)x1 - x0 + 1U;
	height = (uint32_t)y1 - y0 + 1U;

	data->stream_window_set = true;
	data->stream_pixels_remaining = width * height;

	ret = tft_set_address_window_locked(dev, x0, y0, x1, y1);
	tft_release_bus(dev);
	if (ret != 0) {
		data->stream_window_set = false;
		data->stream_pixels_remaining = 0U;
	}

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

static int tft_push_color_impl(const struct device *dev, uint16_t color)
{
	struct tft_drv_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);

	if (!data->stream_window_set || (data->stream_pixels_remaining == 0U)) {
		ret = -EPERM;
		goto out;
	}

	data->tx_buf[0] = (uint8_t)(color >> 8);
	data->tx_buf[1] = (uint8_t)(color & 0xFFU);
	ret = tft_write_data_locked(dev, data->tx_buf, 2U);
	tft_release_bus(dev);

	if (ret == 0) {
		data->stream_pixels_remaining--;
		if (data->stream_pixels_remaining == 0U) {
			data->stream_window_set = false;
		}
	} else {
		data->stream_window_set = false;
		data->stream_pixels_remaining = 0U;
	}

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

static int tft_push_colors_impl(const struct device *dev, const uint16_t *colors,
				size_t len)
{
	struct tft_drv_data *data = dev->data;
	size_t offset = 0U;
	int ret = 0;

	if ((colors == NULL) || (len == 0U)) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	if (!data->stream_window_set || (data->stream_pixels_remaining == 0U)) {
		ret = -EPERM;
		goto out;
	}

	if (len > data->stream_pixels_remaining) {
		ret = -EINVAL;
		goto out;
	}

	while (offset < len) {
		size_t fragment_len = len - offset;

		if (fragment_len > TFT_TX_PIXELS_PER_CHUNK) {
			fragment_len = TFT_TX_PIXELS_PER_CHUNK;
		}

		tft_pack_colors(data->tx_buf, &colors[offset], fragment_len);
		ret = tft_write_data_locked(dev, data->tx_buf, fragment_len * 2U);
		if (ret != 0) {
			break;
		}

		offset += fragment_len;
	}

	tft_release_bus(dev);

	if (ret == 0) {
		data->stream_pixels_remaining -= len;
		if (data->stream_pixels_remaining == 0U) {
			data->stream_window_set = false;
		}
	} else {
		data->stream_window_set = false;
		data->stream_pixels_remaining = 0U;
	}

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

static int tft_blit_impl(const struct device *dev, uint16_t x, uint16_t y,
			 uint16_t width, uint16_t height,
			 const uint16_t *pixels)
{
	struct tft_drv_data *data = dev->data;
	size_t total_pixels;
	size_t offset = 0U;
	int ret = 0;

	if ((pixels == NULL) || (width == 0U) || (height == 0U)) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	if (data->stream_window_set) {
		ret = -EBUSY;
		goto out;
	}

	if (((uint32_t)x + width) > data->width ||
	    ((uint32_t)y + height) > data->height) {
		ret = -EINVAL;
		goto out;
	}

	ret = tft_set_address_window_locked(dev, x, y, x + width - 1U,
					    y + height - 1U);
	if (ret != 0) {
		goto out;
	}

	total_pixels = (size_t)width * height;
	while (offset < total_pixels) {
		size_t fragment_pixels = total_pixels - offset;

		if (fragment_pixels > TFT_TX_PIXELS_PER_CHUNK) {
			fragment_pixels = TFT_TX_PIXELS_PER_CHUNK;
		}

		tft_pack_colors(data->tx_buf, &pixels[offset], fragment_pixels);

		ret = tft_write_data_locked(dev, data->tx_buf,
					    fragment_pixels * 2U);
		if (ret != 0) {
			break;
		}

		offset += fragment_pixels;
	}

	tft_release_bus(dev);

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

static int tft_fill_area_impl(const struct device *dev, uint16_t x, uint16_t y,
			      uint16_t width, uint16_t height, uint16_t color)
{
	struct tft_drv_data *data = dev->data;
	size_t total_pixels;
	size_t offset = 0U;
	int ret = 0;
	size_t i;

	if ((width == 0U) || (height == 0U)) {
		return 0;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	if (data->stream_window_set) {
		ret = -EBUSY;
		goto out;
	}

	if (((uint32_t)x + width) > data->width ||
	    ((uint32_t)y + height) > data->height) {
		ret = -EINVAL;
		goto out;
	}

	ret = tft_set_address_window_locked(dev, x, y, x + width - 1U,
					    y + height - 1U);
	if (ret != 0) {
		goto out;
	}

	for (i = 0U; i < TFT_TX_PIXELS_PER_CHUNK; i++) {
		data->tx_buf[(i * 2U)] = (uint8_t)(color >> 8);
		data->tx_buf[(i * 2U) + 1U] = (uint8_t)(color & 0xFFU);
	}

	total_pixels = (size_t)width * height;
	while (offset < total_pixels) {
		size_t fragment_pixels = total_pixels - offset;

		if (fragment_pixels > TFT_TX_PIXELS_PER_CHUNK) {
			fragment_pixels = TFT_TX_PIXELS_PER_CHUNK;
		}

		ret = tft_write_data_locked(dev, data->tx_buf,
					    fragment_pixels * 2U);
		if (ret != 0) {
			break;
		}

		offset += fragment_pixels;
	}

	tft_release_bus(dev);

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

static int tft_set_rotation_impl(const struct device *dev, uint8_t rotation)
{
	struct tft_drv_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);

	if (data->stream_window_set) {
		k_mutex_unlock(&data->lock);
		return -EBUSY;
	}

	ret = tft_set_rotation_locked(dev, rotation);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int tft_set_scroll_area_impl(const struct device *dev, uint16_t top,
				    uint16_t height)
{
	struct tft_drv_data *data = dev->data;
	uint16_t bottom;
	uint8_t scroll_data[6];
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);

	if (data->stream_window_set) {
		ret = -EBUSY;
		goto out;
	}

	if ((top > data->height) || (height > data->height) ||
	    ((uint32_t)top + height) > data->height) {
		ret = -EINVAL;
		goto out;
	}

	bottom = data->height - top - height;
	scroll_data[0] = (uint8_t)(top >> 8);
	scroll_data[1] = (uint8_t)(top & 0xFFU);
	scroll_data[2] = (uint8_t)(height >> 8);
	scroll_data[3] = (uint8_t)(height & 0xFFU);
	scroll_data[4] = (uint8_t)(bottom >> 8);
	scroll_data[5] = (uint8_t)(bottom & 0xFFU);

	ret = tft_send_command_locked(dev, TFT_CMD_VSCRDEF, scroll_data,
				      sizeof(scroll_data));

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

static int tft_scroll_impl(const struct device *dev, uint16_t offset)
{
	struct tft_drv_data *data = dev->data;
	uint8_t scroll_data[2];
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);

	if (data->stream_window_set) {
		ret = -EBUSY;
		goto out;
	}

	if (offset >= data->height) {
		ret = -EINVAL;
		goto out;
	}

	scroll_data[0] = (uint8_t)(offset >> 8);
	scroll_data[1] = (uint8_t)(offset & 0xFFU);
	ret = tft_send_command_locked(dev, TFT_CMD_VSCSAD, scroll_data,
				      sizeof(scroll_data));

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

static int tft_set_brightness_impl(const struct device *dev, uint8_t value)
{
	struct tft_drv_data *data = dev->data;
	uint8_t ctrl_data = 0x2CU;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);

	if (data->stream_window_set) {
		ret = -EBUSY;
		goto out;
	}

	ret = tft_send_command_locked(dev, TFT_CMD_WRCTRLD, &ctrl_data, 1U);
	if (ret == 0) {
		ret = tft_send_command_locked(dev, TFT_CMD_WRDISBV, &value, 1U);
	}

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

static uint16_t tft_get_width_impl(const struct device *dev)
{
	struct tft_drv_data *data = dev->data;

	return data->width;
}

static uint16_t tft_get_height_impl(const struct device *dev)
{
	struct tft_drv_data *data = dev->data;

	return data->height;
}

static int tft_drv_init(const struct device *dev)
{
	const struct tft_drv_config *config = dev->config;
	struct tft_drv_data *data = dev->data;
	uint8_t color_mode = 0x55U;
	int ret;

	if (!spi_is_ready_dt(&config->bus)) {
		LOG_ERR("SPI bus is not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&config->dc_gpio)) {
		LOG_ERR("DC GPIO is not ready");
		return -ENODEV;
	}

	if (config->has_reset_gpio && !gpio_is_ready_dt(&config->reset_gpio)) {
		LOG_ERR("Reset GPIO is not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&config->dc_gpio, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		return ret;
	}

	if (config->has_reset_gpio) {
		ret = gpio_pin_configure_dt(&config->reset_gpio,
					    GPIO_OUTPUT_ACTIVE);
		if (ret != 0) {
			return ret;
		}

		(void)gpio_pin_set_dt(&config->reset_gpio, 0);
		k_msleep(1);
		(void)gpio_pin_set_dt(&config->reset_gpio, 1);
		k_msleep(1);
	}

	k_mutex_init(&data->lock);
	tft_update_dimensions(config, data, TFT_ROTATION_0);
	data->stream_window_set = false;
	data->stream_pixels_remaining = 0U;

	ret = tft_send_command_locked(dev, TFT_CMD_SWRESET, NULL, 0U);
	if (ret != 0) {
		return ret;
	}
	k_msleep(50);

	ret = tft_send_command_locked(dev, TFT_CMD_SLPOUT, NULL, 0U);
	if (ret != 0) {
		return ret;
	}
	k_msleep(50);

	ret = tft_send_command_locked(dev,
				      tft_controller_inverted(config->controller) ?
					      TFT_CMD_INVON :
					      TFT_CMD_INVOFF,
				      NULL, 0U);
	if (ret != 0) {
		return ret;
	}
	k_msleep(50);

	ret = tft_send_command_locked(dev, TFT_CMD_COLMOD, &color_mode, 1U);
	if (ret != 0) {
		return ret;
	}
	k_msleep(50);

	ret = tft_set_rotation_locked(dev, config->rotation);
	if (ret != 0) {
		return ret;
	}
	k_msleep(100);

	ret = tft_send_command_locked(dev, TFT_CMD_DISPON, NULL, 0U);
	if (ret != 0) {
		return ret;
	}

	k_msleep(5);
	return 0;
}

static const tft_drv_api_t api = {
	.set_address_window = tft_set_address_window_impl,
	.push_color = tft_push_color_impl,
	.push_colors = tft_push_colors_impl,
	.blit = tft_blit_impl,
	.fill_area = tft_fill_area_impl,
	.set_rotation = tft_set_rotation_impl,
	.set_scroll_area = tft_set_scroll_area_impl,
	.scroll = tft_scroll_impl,
	.set_brightness = tft_set_brightness_impl,
	.get_width = tft_get_width_impl,
	.get_height = tft_get_height_impl,
};

#define TFT_CONTROLLER_ILI9341 TFT_CTRL_ILI9341
#define TFT_CONTROLLER_ST7789 TFT_CTRL_ST7789
#define TFT_CONTROLLER_ST7796 TFT_CTRL_ST7796
#define TFT_CONTROLLER_FROM_DT(inst) \
	UTIL_CAT(TFT_CONTROLLER_, DT_INST_STRING_UPPER_TOKEN(inst, controller))

#define TFT_DRV_DEFINE(inst)                                                    \
	static struct tft_drv_data tft_drv_data_##inst;                         \
	static const struct tft_drv_config tft_drv_config_##inst = {            \
		.bus = SPI_DT_SPEC_INST_GET(                                    \
			inst, SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB |           \
				      SPI_WORD_SET(8) | SPI_HOLD_ON_CS |      \
				      SPI_LOCK_ON),                          \
		.dc_gpio = GPIO_DT_SPEC_INST_GET(inst, dc_gpios),               \
		.reset_gpio =                                                    \
			GPIO_DT_SPEC_INST_GET_OR(inst, reset_gpios, {0}),        \
		.native_width = DT_INST_PROP(inst, width),                      \
		.native_height = DT_INST_PROP(inst, height),                    \
		.rotation = DT_INST_PROP_OR(inst, rotation, 0),                 \
		.controller = TFT_CONTROLLER_FROM_DT(inst),                     \
		.has_reset_gpio = DT_INST_NODE_HAS_PROP(inst, reset_gpios),     \
	};                                                                   \
	DEVICE_DT_INST_DEFINE(inst, tft_drv_init, NULL,                       \
			      &tft_drv_data_##inst, &tft_drv_config_##inst,   \
			      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, \
			      &api);

DT_INST_FOREACH_STATUS_OKAY(TFT_DRV_DEFINE)
