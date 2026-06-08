/*
 * Copyright (c) 2026 Surya Poudel
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SUBSYS_TFT_GFX_H_
#define ZEPHYR_SUBSYS_TFT_GFX_H_

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/device.h>

#include <zephyr_oot/drivers/tft_drv.h>
#include <zephyr_oot/subsys/tft_fonts.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TFT_COLOR_RED 0xF800U
#define TFT_COLOR_GREEN 0x07E0U
#define TFT_COLOR_BLUE 0x001FU
#define TFT_COLOR_WHITE 0xFFFFU
#define TFT_COLOR_BLACK 0x0000U
#define TFT_COLOR_YELLOW 0xFFE0U
#define TFT_COLOR_CYAN 0x07FFU
#define TFT_COLOR_MAGENTA 0xF81FU
#define TFT_COLOR_GRAY 0x8410U
#define TFT_COLOR_ORANGE 0xFC00U
#define TFT_COLOR_PINK 0xFDF9U
#define TFT_COLOR_BROWN 0x59E0U
#define TFT_COLOR_PURPLE 0x780FU
#define TFT_COLOR_GOLD 0xFEA0U
#define TFT_COLOR_SILVER 0xC618U
#define TFT_COLOR_LIME 0x87E0U
#define TFT_COLOR_NAVY 0x000FU
#define TFT_COLOR_TEAL 0x0410U
#define TFT_COLOR_MAROON 0x8000U
#define TFT_COLOR_OLIVE 0x8400U
#define TFT_COLOR_VIOLET 0x915CU
#define TFT_COLOR_SKYBLUE 0x867FU

void tft_set_bg_color(uint16_t bg_color);
uint16_t tft_get_bg_color(void);

uint16_t tft_get_width(const struct device *dev);
uint16_t tft_get_height(const struct device *dev);

int tft_set_rotation(const struct device *dev, uint8_t rotation);
int tft_set_scroll_area(const struct device *dev, uint16_t top,
			uint16_t height);
int tft_scroll(const struct device *dev, uint16_t offset);
int tft_set_brightness(const struct device *dev, uint8_t value);

int tft_fill_screen(const struct device *dev, uint16_t color);
int tft_clear_screen(const struct device *dev);
int tft_draw_pixel(const struct device *dev, uint16_t x, uint16_t y,
		   uint16_t color);
int tft_draw_image(const struct device *dev, uint16_t x, uint16_t y,
		   uint16_t width, uint16_t height,
		   const uint16_t *color_buf);

void tft_draw_text(const struct device *dev, const char *text, uint16_t x,
		   uint16_t y, uint16_t color, const uint8_t *font_data);
void tft_draw_text_transparent(const struct device *dev, const char *text,
			       uint16_t x, uint16_t y, uint16_t color,
			       const uint8_t *font_data);

void tft_draw_line(const struct device *dev, uint16_t x0, uint16_t y0,
		   uint16_t x1, uint16_t y1, uint16_t color);
void tft_draw_rectangle(const struct device *dev, uint16_t x0, uint16_t y0,
			uint16_t width, uint16_t height, uint16_t color);
void tft_fill_rectangle(const struct device *dev, uint16_t x0, uint16_t y0,
			uint16_t width, uint16_t height, uint16_t color);
void tft_draw_circle(const struct device *dev, uint16_t xc, uint16_t yc,
		     uint16_t r, uint16_t color);
void tft_fill_circle(const struct device *dev, uint16_t xc, uint16_t yc,
		     uint16_t r, uint16_t color);
void tft_draw_triangle(const struct device *dev, int x0, int y0, int x1, int y1,
		       int x2, int y2, uint16_t color);
void tft_fill_triangle(const struct device *dev, int x1, int y1, int x2, int y2,
		       int x3, int y3, uint16_t color);
void tft_draw_rounded_rectangle(const struct device *dev, int x, int y, int w,
				int h, int r, uint16_t color);
void tft_fill_rounded_rectangle(const struct device *dev, uint16_t x,
				uint16_t y, uint16_t w, uint16_t h,
				uint16_t r, uint16_t color);
void tft_draw_scroll_text(const struct device *dev, const char *text,
			  uint16_t color, const uint8_t *font_data);
void tft_draw_folder_icon(const struct device *dev, uint16_t x, uint16_t y,
			  const char *label);
void tft_draw_file_icon(const struct device *dev, uint16_t x, uint16_t y,
			const char *ext, const char *label);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SUBSYS_TFT_GFX_H_ */
