/*
 * Copyright (c) 2026 Surya Poudel
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include <zephyr_oot/subsys/tft_gfx.h>

#define TFT_GFX_MAX_GLYPH_PIXELS 512U

static uint16_t s_bg_color = TFT_COLOR_BLACK;
static bool s_scroll_enabled;
static uint16_t s_scroll_row;

static bool tft_plot_i(const struct device *dev, int x, int y, uint16_t color)
{
	uint16_t width = tft_get_width(dev);
	uint16_t height = tft_get_height(dev);

	if ((x < 0) || (y < 0) || (x >= width) || (y >= height)) {
		return false;
	}

	(void)tft_draw_pixel(dev, (uint16_t)x, (uint16_t)y, color);
	return true;
}

static inline void tft_swap_int(int *a, int *b)
{
	int temp = *a;

	*a = *b;
	*b = temp;
}

static inline int tft_abs_int(int value)
{
	return (value < 0) ? -value : value;
}

static void tft_draw_fast_hline(const struct device *dev, int x0, int x1, int y,
				uint16_t color)
{
	int max_x = (int)tft_get_width(dev) - 1;
	int max_y = (int)tft_get_height(dev) - 1;

	if (x0 > x1) {
		tft_swap_int(&x0, &x1);
	}

	if ((y < 0) || (y > max_y) || (x1 < 0) || (x0 > max_x)) {
		return;
	}

	if (x0 < 0) {
		x0 = 0;
	}
	if (x1 > max_x) {
		x1 = max_x;
	}

	tft_fill_rectangle(dev, (uint16_t)x0, (uint16_t)y,
			   (uint16_t)(x1 - x0 + 1), 1U, color);
}

static void tft_draw_fast_vline(const struct device *dev, int x, int y0, int y1,
				uint16_t color)
{
	int max_x = (int)tft_get_width(dev) - 1;
	int max_y = (int)tft_get_height(dev) - 1;

	if (y0 > y1) {
		tft_swap_int(&y0, &y1);
	}

	if ((x < 0) || (x > max_x) || (y1 < 0) || (y0 > max_y)) {
		return;
	}

	if (y0 < 0) {
		y0 = 0;
	}
	if (y1 > max_y) {
		y1 = max_y;
	}

	tft_fill_rectangle(dev, (uint16_t)x, (uint16_t)y0, 1U,
			   (uint16_t)(y1 - y0 + 1), color);
}

static void tft_draw_char(const struct device *dev, uint16_t x, uint16_t y,
			  char c, uint16_t color, const uint8_t *font_data,
			  bool transparent)
{
	tft_font_t font = {0};
	uint16_t glyph[TFT_GFX_MAX_GLYPH_PIXELS];
	uint16_t glyph_width;
	uint32_t max_pixels;
	uint8_t column;
	uint8_t row;

	if ((font_data == NULL) || (c < 32) || (c > 127)) {
		return;
	}

	font.offset = font_data[0];
	font.width = font_data[1];
	font.height = font_data[2];
	font.bpl = font_data[3];
	font.data = &font_data[4];

	max_pixels = (uint32_t)font.width * font.height;
	if (max_pixels > TFT_GFX_MAX_GLYPH_PIXELS) {
		return;
	}

	c -= 32;
	glyph_width = font.data[((uint8_t)c) * font.offset];

	for (column = 0U; column < font.width; column++) {
		uint32_t line = 0U;
		uint8_t byte_idx;

		for (byte_idx = 0U; byte_idx < font.bpl; byte_idx++) {
			uint32_t font_idx =
				((uint32_t)c * font.offset) +
				((uint32_t)column * font.bpl) + byte_idx + 1U;

			line |= (uint32_t)font.data[font_idx] << (byte_idx << 3);
		}

		for (row = 0U; row < font.height; row++) {
			bool set_pixel = (line & 0x01U) != 0U;

				if (transparent) {
					if (set_pixel) {
						(void)tft_draw_pixel(dev, x + column,
								    y + row, color);
					}
				} else {
					glyph[(row * font.width) + column] =
						set_pixel ? color : s_bg_color;
				}

			line >>= 1;
		}
	}

	if (!transparent) {
		(void)tft_draw_image(dev, x, y, font.width, font.height, glyph);
	}

	(void)glyph_width;
}

static uint16_t tft_char_advance(char c, const uint8_t *font_data)
{
	uint8_t offset = font_data[0];
	uint8_t width = font_data[1];
	uint8_t char_width;
	const uint8_t *glyph_data = &font_data[4];

	if ((c < 32) || (c > 127)) {
		return width;
	}

	char_width = glyph_data[((uint8_t)c - 32U) * offset];

	if ((uint16_t)char_width + 2U < width) {
		return (uint16_t)char_width + 2U;
	}

	return width;
}

static void tft_fill_circle_helper(const struct device *dev, uint16_t x0,
				   uint16_t y0, uint16_t r,
				   uint16_t corner_mask, uint16_t delta,
				   uint16_t color)
{
	int32_t f = 1 - r;
	int32_t ddx = 1;
	int32_t ddy = -2 * (int32_t)r;
	int32_t y = 0;
	int32_t cur_r = r;

	delta++;

	while (y < cur_r) {
		if (f >= 0) {
			if ((corner_mask & 0x1U) != 0U) {
				tft_draw_fast_hline(dev, x0 - y,
						    x0 + y + delta - 1,
						    y0 + cur_r, color);
			}
			if ((corner_mask & 0x2U) != 0U) {
				tft_draw_fast_hline(dev, x0 - y,
						    x0 + y + delta - 1,
						    y0 - cur_r, color);
			}
			cur_r--;
			ddy += 2;
			f += ddy;
		}

		y++;
		ddx += 2;
		f += ddx;

		if ((corner_mask & 0x1U) != 0U) {
			tft_draw_fast_hline(dev, x0 - cur_r,
					    x0 + cur_r + delta - 1, y0 + y,
					    color);
		}
		if ((corner_mask & 0x2U) != 0U) {
			tft_draw_fast_hline(dev, x0 - cur_r,
					    x0 + cur_r + delta - 1, y0 - y,
					    color);
		}
	}
}

static void tft_draw_circle_quadrant(const struct device *dev, int xc, int yc,
				     int r, uint16_t color, int quadrant)
{
	int x = 0;
	int y = r;
	int d = 3 - (2 * r);

	while (x <= y) {
		if (quadrant == 0) {
			tft_plot_i(dev, xc + x, yc - y, color);
			tft_plot_i(dev, xc + y, yc - x, color);
		}
		if (quadrant == 1) {
			tft_plot_i(dev, xc - x, yc - y, color);
			tft_plot_i(dev, xc - y, yc - x, color);
		}
		if (quadrant == 2) {
			tft_plot_i(dev, xc - x, yc + y, color);
			tft_plot_i(dev, xc - y, yc + x, color);
		}
		if (quadrant == 3) {
			tft_plot_i(dev, xc + x, yc + y, color);
			tft_plot_i(dev, xc + y, yc + x, color);
		}

		x++;
		if (d > 0) {
			y--;
			d += 4 * (x - y) + 10;
		} else {
			d += 4 * x + 6;
		}
	}
}

static void tft_fill_flat_bottom_triangle(const struct device *dev, int x1,
					  int y1, int x2, int y2, int x3,
					  int y3, uint16_t color)
{
	float invslope1 = (float)(x2 - x1) / (float)(y2 - y1);
	float invslope2 = (float)(x3 - x1) / (float)(y3 - y1);
	float x_start = (float)x1;
	float x_end = (float)x1;
	int y;

	for (y = y1; y <= y3; y++) {
		tft_draw_fast_hline(dev, (int)x_start, (int)x_end, y, color);
		x_start += invslope1;
		x_end += invslope2;
	}
}

static void tft_fill_flat_top_triangle(const struct device *dev, int x1, int y1,
				       int x2, int y2, int x3, int y3,
				       uint16_t color)
{
	float invslope1 = (float)(x3 - x1) / (float)(y3 - y1);
	float invslope2 = (float)(x3 - x2) / (float)(y3 - y2);
	float x_start = (float)x3;
	float x_end = (float)x3;
	int y;

	for (y = y3; y >= y1; y--) {
		tft_draw_fast_hline(dev, (int)x_start, (int)x_end, y, color);
		x_start -= invslope1;
		x_end -= invslope2;
	}
}

void tft_set_bg_color(uint16_t bg_color)
{
	s_bg_color = bg_color;
}

uint16_t tft_get_bg_color(void)
{
	return s_bg_color;
}

uint16_t tft_get_width(const struct device *dev)
{
	return tft_drv_get_width(dev);
}

uint16_t tft_get_height(const struct device *dev)
{
	return tft_drv_get_height(dev);
}

int tft_set_rotation(const struct device *dev, uint8_t rotation)
{
	return tft_drv_set_rotation(dev, rotation);
}

int tft_set_scroll_area(const struct device *dev, uint16_t top,
			uint16_t height)
{
	return tft_drv_set_scroll_area(dev, top, height);
}

int tft_scroll(const struct device *dev, uint16_t offset)
{
	return tft_drv_scroll(dev, offset);
}

int tft_set_brightness(const struct device *dev, uint8_t value)
{
	return tft_drv_set_brightness(dev, value);
}

int tft_fill_screen(const struct device *dev, uint16_t color)
{
	return tft_drv_fill_area(dev, 0U, 0U, tft_get_width(dev),
				 tft_get_height(dev), color);
}

int tft_clear_screen(const struct device *dev)
{
	return tft_fill_screen(dev, s_bg_color);
}

int tft_draw_pixel(const struct device *dev, uint16_t x, uint16_t y,
		   uint16_t color)
{
	return tft_drv_fill_area(dev, x, y, 1U, 1U, color);
}

int tft_draw_image(const struct device *dev, uint16_t x, uint16_t y,
		   uint16_t width, uint16_t height,
		   const uint16_t *color_buf)
{
	return tft_drv_blit(dev, x, y, width, height, color_buf);
}

void tft_draw_text(const struct device *dev, const char *text, uint16_t x,
		   uint16_t y, uint16_t color, const uint8_t *font_data)
{
	if ((text == NULL) || (font_data == NULL)) {
		return;
	}

	while (*text != '\0') {
		tft_draw_char(dev, x, y, *text, color, font_data, false);
		x += tft_char_advance(*text, font_data);
		text++;
	}
}

void tft_draw_text_transparent(const struct device *dev, const char *text,
			       uint16_t x, uint16_t y, uint16_t color,
			       const uint8_t *font_data)
{
	if ((text == NULL) || (font_data == NULL)) {
		return;
	}

	while (*text != '\0') {
		tft_draw_char(dev, x, y, *text, color, font_data, true);
		x += tft_char_advance(*text, font_data);
		text++;
	}
}

void tft_draw_line(const struct device *dev, uint16_t x0, uint16_t y0,
		   uint16_t x1, uint16_t y1, uint16_t color)
{
	if (x0 == x1) {
		tft_draw_fast_vline(dev, x0, y0, y1, color);
		return;
	}

	if (y0 == y1) {
		tft_draw_fast_hline(dev, x0, x1, y0, color);
		return;
	}

	{
		int dx = tft_abs_int((int)x1 - (int)x0);
		int sx = (x0 < x1) ? 1 : -1;
		int dy = -tft_abs_int((int)y1 - (int)y0);
		int sy = (y0 < y1) ? 1 : -1;
		int err = dx + dy;
		int cur_x = x0;
		int cur_y = y0;
		int e2;

		while (true) {
			tft_plot_i(dev, cur_x, cur_y, color);

			if ((cur_x == x1) && (cur_y == y1)) {
				break;
			}

			e2 = 2 * err;
			if (e2 >= dy) {
				err += dy;
				cur_x += sx;
			}
			if (e2 <= dx) {
				err += dx;
				cur_y += sy;
			}
		}
	}
}

void tft_draw_rectangle(const struct device *dev, uint16_t x0, uint16_t y0,
			uint16_t width, uint16_t height, uint16_t color)
{
	if ((width == 0U) || (height == 0U)) {
		return;
	}

	tft_draw_line(dev, x0, y0, x0 + width - 1U, y0, color);
	tft_draw_line(dev, x0, y0, x0, y0 + height - 1U, color);
	tft_draw_line(dev, x0 + width - 1U, y0, x0 + width - 1U,
		      y0 + height - 1U, color);
	tft_draw_line(dev, x0, y0 + height - 1U, x0 + width - 1U,
		      y0 + height - 1U, color);
}

void tft_fill_rectangle(const struct device *dev, uint16_t x0, uint16_t y0,
			uint16_t width, uint16_t height, uint16_t color)
{
	(void)tft_drv_fill_area(dev, x0, y0, width, height, color);
}

void tft_draw_circle(const struct device *dev, uint16_t xc, uint16_t yc,
		     uint16_t r, uint16_t color)
{
	int x = 0;
	int y = r;
	int d = 3 - (2 * (int)r);

	while (x <= y) {
		tft_plot_i(dev, xc + x, yc + y, color);
		tft_plot_i(dev, xc - x, yc + y, color);
		tft_plot_i(dev, xc + x, yc - y, color);
		tft_plot_i(dev, xc - x, yc - y, color);
		tft_plot_i(dev, xc + y, yc + x, color);
		tft_plot_i(dev, xc - y, yc + x, color);
		tft_plot_i(dev, xc + y, yc - x, color);
		tft_plot_i(dev, xc - y, yc - x, color);

		x++;
		if (d > 0) {
			y--;
			d += 4 * (x - y) + 10;
		} else {
			d += 4 * x + 6;
		}
	}
}

void tft_fill_circle(const struct device *dev, uint16_t xc, uint16_t yc,
		     uint16_t r, uint16_t color)
{
	int x = 0;
	int y = r;
	int d = 3 - (2 * (int)r);

	while (x <= y) {
		tft_draw_fast_hline(dev, xc - x, xc + x, yc + y, color);
		tft_draw_fast_hline(dev, xc - x, xc + x, yc - y, color);
		tft_draw_fast_hline(dev, xc - y, xc + y, yc + x, color);
		tft_draw_fast_hline(dev, xc - y, xc + y, yc - x, color);

		x++;
		if (d > 0) {
			y--;
			d += 4 * (x - y) + 10;
		} else {
			d += 4 * x + 6;
		}
	}
}

void tft_draw_triangle(const struct device *dev, int x0, int y0, int x1, int y1,
		       int x2, int y2, uint16_t color)
{
	tft_draw_line(dev, (uint16_t)x0, (uint16_t)y0, (uint16_t)x1,
		      (uint16_t)y1, color);
	tft_draw_line(dev, (uint16_t)x1, (uint16_t)y1, (uint16_t)x2,
		      (uint16_t)y2, color);
	tft_draw_line(dev, (uint16_t)x2, (uint16_t)y2, (uint16_t)x0,
		      (uint16_t)y0, color);
}

void tft_fill_triangle(const struct device *dev, int x1, int y1, int x2, int y2,
		       int x3, int y3, uint16_t color)
{
	if (y1 > y2) {
		tft_swap_int(&x1, &x2);
		tft_swap_int(&y1, &y2);
	}
	if (y1 > y3) {
		tft_swap_int(&x1, &x3);
		tft_swap_int(&y1, &y3);
	}
	if (y2 > y3) {
		tft_swap_int(&x2, &x3);
		tft_swap_int(&y2, &y3);
	}

	if (y2 == y3) {
		tft_fill_flat_bottom_triangle(dev, x1, y1, x2, y2, x3, y3,
					      color);
	} else if (y1 == y2) {
		tft_fill_flat_top_triangle(dev, x1, y1, x2, y2, x3, y3,
					   color);
	} else {
		int x4 = x1 + (int)((float)(y2 - y1) / (float)(y3 - y1) *
				    (float)(x3 - x1));
		int y4 = y2;

		tft_fill_flat_bottom_triangle(dev, x1, y1, x2, y2, x4, y4,
					      color);
		tft_fill_flat_top_triangle(dev, x2, y2, x4, y4, x3, y3,
					   color);
	}
}

void tft_draw_rounded_rectangle(const struct device *dev, int x, int y, int w,
				int h, int r, uint16_t color)
{
	tft_draw_fast_hline(dev, x + r, x + w - r - 1, y, color);
	tft_draw_fast_hline(dev, x + r, x + w - r - 1, y + h - 1, color);
	tft_draw_fast_vline(dev, x, y + r, y + h - r - 1, color);
	tft_draw_fast_vline(dev, x + w - 1, y + r, y + h - r - 1, color);

	tft_draw_circle_quadrant(dev, x + r, y + r, r, color, 1);
	tft_draw_circle_quadrant(dev, x + w - r - 1, y + r, r, color, 0);
	tft_draw_circle_quadrant(dev, x + r, y + h - r - 1, r, color, 2);
	tft_draw_circle_quadrant(dev, x + w - r - 1, y + h - r - 1, r, color,
				 3);
}

void tft_fill_rounded_rectangle(const struct device *dev, uint16_t x,
				uint16_t y, uint16_t w, uint16_t h,
				uint16_t r, uint16_t color)
{
	tft_fill_rectangle(dev, x, y + r, w, h - (2U * r), color);
	tft_fill_circle_helper(dev, x + r, y + h - r - 1U, r, 1U,
			       w - (2U * r) - 1U, color);
	tft_fill_circle_helper(dev, x + r, y + r, r, 2U,
			       w - (2U * r) - 1U, color);
}

void tft_draw_scroll_text(const struct device *dev, const char *text,
			  uint16_t color, const uint8_t *font_data)
{
	uint16_t font_height;
	uint16_t panel_width = tft_get_width(dev);
	uint16_t panel_height = tft_get_height(dev);

	if ((text == NULL) || (font_data == NULL)) {
		return;
	}

	font_height = font_data[2];
	tft_fill_rectangle(dev, 0U, s_scroll_row, panel_width, font_height,
			   s_bg_color);
	tft_draw_text(dev, text, 0U, s_scroll_row, color, font_data);

	s_scroll_row += font_height;
	if (s_scroll_row >= panel_height) {
		s_scroll_row = 0U;
		s_scroll_enabled = true;
	}

	if (s_scroll_enabled) {
		(void)tft_scroll(dev, s_scroll_row);
	}
}

void tft_draw_folder_icon(const struct device *dev, uint16_t x, uint16_t y,
			  const char *label)
{
	uint16_t folder_color = 0xFEC0U;
	uint16_t tab_color = 0xECA0U;

	tft_fill_rectangle(dev, x, y + 5U, 40U, 25U, folder_color);
	tft_fill_rectangle(dev, x, y, 15U, 5U, tab_color);
	tft_fill_triangle(dev, x + 15U, y, x + 15U, y + 4U, x + 20U, y + 4U,
			  tab_color);
	tft_draw_text(dev, label, x, y + 35U, TFT_COLOR_BLACK, FONT1);
}

void tft_draw_file_icon(const struct device *dev, uint16_t x, uint16_t y,
			const char *ext, const char *label)
{
	tft_draw_fast_hline(dev, x, x + 20U, y, TFT_COLOR_BLACK);
	tft_draw_fast_vline(dev, x, y, y + 30U, TFT_COLOR_BLACK);
	tft_draw_fast_hline(dev, x, x + 30U, y + 30U, TFT_COLOR_BLACK);
	tft_draw_fast_vline(dev, x + 30U, y + 10U, y + 30U, TFT_COLOR_BLACK);
	tft_fill_triangle(dev, x + 20U, y, x + 20U, y + 10U, x + 30U, y + 10U,
			  TFT_COLOR_BLACK);
	tft_draw_text(dev, ext, x + 3U, y + 15U, TFT_COLOR_BLACK, FONT1);
	tft_draw_text(dev, label, x, y + 35U, TFT_COLOR_BLACK, FONT1);
}
