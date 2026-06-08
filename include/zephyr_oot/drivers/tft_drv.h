/*
 * Copyright (c) 2026 Surya Poudel
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_TFT_DRV_H_
#define ZEPHYR_DRIVERS_TFT_DRV_H_

#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	TFT_ROTATION_0 = 0U,
	TFT_ROTATION_90 = 1U,
	TFT_ROTATION_180 = 2U,
	TFT_ROTATION_270 = 3U,
} tft_rotation_t;

typedef struct {
	int (*set_address_window)(const struct device *dev, uint16_t x0,
				  uint16_t y0, uint16_t x1, uint16_t y1);
	int (*push_color)(const struct device *dev, uint16_t color);
	int (*push_colors)(const struct device *dev, const uint16_t *colors,
			   size_t len);
	int (*blit)(const struct device *dev, uint16_t x, uint16_t y,
		    uint16_t width, uint16_t height,
		    const uint16_t *pixels);
	int (*fill_area)(const struct device *dev, uint16_t x, uint16_t y,
			 uint16_t width, uint16_t height, uint16_t color);
	int (*set_rotation)(const struct device *dev, uint8_t rotation);
	int (*set_scroll_area)(const struct device *dev, uint16_t top,
			       uint16_t height);
	int (*scroll)(const struct device *dev, uint16_t offset);
	int (*set_brightness)(const struct device *dev, uint8_t value);
	uint16_t (*get_width)(const struct device *dev);
	uint16_t (*get_height)(const struct device *dev);
} tft_drv_api_t;

/*
 * set_address_window() arms a pixel stream for the selected region.
 * push_color() and push_colors() append RGB565 pixel data into that
 * region until it is fully written.
 */
static inline int tft_drv_set_address_window(const struct device *dev,
					     uint16_t x0, uint16_t y0,
					     uint16_t x1, uint16_t y1)
{
	const tft_drv_api_t *api = dev->api;

	return api->set_address_window(dev, x0, y0, x1, y1);
}

static inline int tft_drv_push_color(const struct device *dev, uint16_t color)
{
	const tft_drv_api_t *api = dev->api;

	return api->push_color(dev, color);
}

static inline int tft_drv_push_colors(const struct device *dev,
				      const uint16_t *colors, size_t len)
{
	const tft_drv_api_t *api = dev->api;

	return api->push_colors(dev, colors, len);
}

static inline int tft_drv_blit(const struct device *dev, uint16_t x, uint16_t y,
			       uint16_t width, uint16_t height,
			       const uint16_t *pixels)
{
	const tft_drv_api_t *api = dev->api;

	return api->blit(dev, x, y, width, height, pixels);
}

static inline int tft_drv_fill_area(const struct device *dev, uint16_t x,
				    uint16_t y, uint16_t width,
				    uint16_t height, uint16_t color)
{
	const tft_drv_api_t *api = dev->api;

	return api->fill_area(dev, x, y, width, height, color);
}

static inline int tft_drv_set_rotation(const struct device *dev,
				       uint8_t rotation)
{
	const tft_drv_api_t *api = dev->api;

	return api->set_rotation(dev, rotation);
}

static inline int tft_drv_set_scroll_area(const struct device *dev,
					  uint16_t top, uint16_t height)
{
	const tft_drv_api_t *api = dev->api;

	return api->set_scroll_area(dev, top, height);
}

static inline int tft_drv_scroll(const struct device *dev, uint16_t offset)
{
	const tft_drv_api_t *api = dev->api;

	return api->scroll(dev, offset);
}

static inline int tft_drv_set_brightness(const struct device *dev,
					 uint8_t value)
{
	const tft_drv_api_t *api = dev->api;

	return api->set_brightness(dev, value);
}

static inline uint16_t tft_drv_get_width(const struct device *dev)
{
	const tft_drv_api_t *api = dev->api;

	return api->get_width(dev);
}

static inline uint16_t tft_drv_get_height(const struct device *dev)
{
	const tft_drv_api_t *api = dev->api;

	return api->get_height(dev);
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_TFT_DRV_H_ */
