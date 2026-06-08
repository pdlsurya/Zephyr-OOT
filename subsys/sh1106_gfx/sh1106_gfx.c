/**
 * @file sh1106_gfx.c
 * @author Surya Poudel
 * @brief SH1106 oled graphics library
 * @version 0.1
 * @date 2023-06-11
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <zephyr_oot/subsys/sh1106_gfx.h>
#include "sh1106_font.h"

#define min(x, y) ((x) < (y) ? (x) : (y))

#define max(x, y) ((x) > (y) ? (x) : (y))

#define PI 3.1416

#define sq(x) ((x) * (x))

#define FRAME_BUFFER ((uint8_t (*)[128])dev->data)

static uint8_t log_row;
static bool log_scroll;

uint8_t char_cnt;
uint8_t num_cnt;
uint8_t battery[14] = {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
		       0x81, 0x81, 0x81, 0x81, 0x81, 0xC3, 0x3C};
uint8_t signal[9] = {0x80, 0x00, 0xC0, 0x00, 0xF0, 0x00, 0xFC, 0x00, 0xFF};
uint8_t bar[128];

uint8_t disp_row;
uint8_t disp_column;

void oled_displayOn(const struct device *dev)
{
	const sh1106_api *api = dev->api;
	api->display_on(dev);
}

void oled_setCursor(uint8_t x, uint8_t y)
{

	disp_column = x;
	disp_row = y / 8;
}

void oled_clearPart(const struct device *dev, uint8_t page, uint8_t start_pos, uint8_t end_pos)
{
	oled_setCursor(start_pos, page * 8);
	for (uint8_t i = start_pos; i <= end_pos; i++) {
		FRAME_BUFFER[disp_row][disp_column++] = 0x00;
	}
}
void oled_display(const struct device *dev)
{
	const sh1106_api *api = dev->api;
	api->write_buffer(dev);
}

void oled_clearDisplay(const struct device *dev)
{
	for (uint8_t page = 0; page < 8; page++) {
		for (uint8_t column = 0; column < 128; column++) {
			FRAME_BUFFER[page][column] = 0;
		}
	}
}

void oled_printChar(const struct device *dev, char C, uint8_t xpos, uint8_t ypos, uint8_t font_size,
		    bool Highlight)
{
	char chr;

	if (font_size == 6) {
		for (uint8_t i = 0; i < 6; i++) {
			chr = font6x8[((int)C - 32) * 6 + i];
			if (Highlight) {
				chr = ~chr;
			}

			FRAME_BUFFER[disp_row][disp_column++] = chr;
		}
	} else if (font_size == 16) {
		oled_setCursor(xpos + (char_cnt)*8, ypos);

		for (uint8_t i = 0; i < 8; i++) {
			chr = font16x8[((int)C - 32) * 16 + i];
			if (Highlight) {
				chr = ~chr;
			}
			FRAME_BUFFER[disp_row][disp_column++] = chr;
		}

		oled_setCursor(xpos + (char_cnt)*8, ypos + 8);

		for (uint8_t i = 8; i < 16; i++) {
			chr = font16x8[((int)C - 32) * 16 + i];
			if (Highlight) {
				chr = ~chr;
			}

			FRAME_BUFFER[disp_row][disp_column++] = chr;
		}

		char_cnt++;
	} else {
		return;
	}
}

void oled_printString(const struct device *dev, const char *str, uint8_t xpos, uint8_t ypos,
		      uint8_t font_size, bool Highlight)
{
	uint8_t len = strlen(str);
	char_cnt = 0;
	oled_setCursor(xpos, ypos);
	for (uint8_t i = 0; i < len; i++) {
		oled_printChar(dev, str[i], xpos, ypos, font_size, Highlight);
	}
	char_cnt = 0;
}

void oled_print7Seg_digit(const struct device *dev, char C, uint8_t xpos, uint8_t ypos)
{
	char chr;

	for (uint8_t column = 0; column < 16; column++) {
		for (uint8_t page = 0; page < 3; page++) {
			oled_setCursor(xpos + column + (num_cnt * 16), ypos + page * 8);
			chr = font_7SEG[((int)C - 48) * 48 + (page + column * 3)];
			FRAME_BUFFER[disp_row][disp_column++] = chr;
		}
	}

	num_cnt++;
}

void oled_print7Seg_number(const struct device *dev, const char *str, uint8_t xpos, uint8_t ypos)
{
	uint8_t len = strlen(str);
	num_cnt = 0;
	oled_setCursor(xpos, ypos);

	for (uint8_t i = 0; i < len; i++) {
		oled_print7Seg_digit(dev, str[i], xpos, ypos);
	}
	num_cnt = 0;
}

void oled_setContrast(const struct device *dev, uint8_t level)
{
	const sh1106_api *api = dev->api;
	api->set_contrast(dev, level);
}

void oled_writeByte(const struct device *dev, uint8_t byte)
{
	
	FRAME_BUFFER[disp_row][disp_column++] = byte;
}

void oled_setPixel(const struct device *dev, uint8_t x, uint8_t y, bool set)
{
	
	uint8_t curDisp;
	uint8_t shift = y % 8;

	oled_setCursor(x, y);
	curDisp = FRAME_BUFFER[disp_row][disp_column];
	if (set) {
		FRAME_BUFFER[disp_row][disp_column] = (curDisp | (0x01 << shift));
	} else {
		FRAME_BUFFER[disp_row][disp_column] = (curDisp & (~(0x01 << shift)));
	}
}

void oled_drawLine(const struct device *dev, float x1, float y1, float x2, float y2, bool set)
{
	// y=mx+c

	if (x1 == x2) // Vertical line has undefined slope;hence, plot without calculating the slope
	{
		for (uint8_t i = min(y1, y2); i <= max(y1, y2); i++) {
			oled_setPixel(dev, x1, i, set);
		}

		return;
	}
	float slope = ((y2 - y1) / (x2 - x1));
	float c = y1 - slope * x1;

	for (float i = min(x1, x2); i <= max(x1, x2); i++) {
		oled_setPixel(dev, i, slope * i + c, set);
	}

	for (float i = min(y1, y2); i <= max(y1, y2); i++) {
		oled_setPixel(dev, (i - c) / slope, i, set);
	}
}

void oled_drawRectangle(const struct device *dev, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
			bool set)
{

	oled_drawLine(dev, x1, y1, x1, y2, set);

	oled_drawLine(dev, x2, y1, x2, y2, set);

	oled_drawLine(dev, x1, y1, x2, y1, set);

	oled_drawLine(dev, x1, y2, x2, y2, set);
}

void oled_plot(const struct device *dev, float current_x, float current_y)
{
	static uint8_t prev_x = 0;
	static float prev_y = 0;

	oled_drawLine(dev, current_x, current_y, prev_x, prev_y, true);
	prev_y = current_y;
	prev_x = current_x;

	if (prev_x >= 127) {
		prev_x = 0;
	}
}

void oled_drawCircle(const struct device *dev, float Cx, float Cy, float radius, bool set)
{
	//(x-cx)^2+(y-cy)^2=r^2

	for (uint8_t i = (Cx - radius); i <= (Cx + radius); i++) {
		oled_setPixel(dev, i, (sqrt(sq(radius) - sq(i - Cx)) + Cy), set);
		oled_setPixel(dev, i, (Cy - sqrt(sq(radius) - sq(i - Cx))), set);
	}

	for (uint8_t i = (Cy - radius); i <= (Cy + radius); i++) {
		oled_setPixel(dev, (sqrt(sq(radius) - sq(i - Cy)) + Cx), i, set);
		oled_setPixel(dev, Cx - (sqrt(sq(radius) - sq(i - Cy))), i, set);
	}
}

void oled_displayBmp(const struct device *dev, const uint8_t *binArray)
{
	
	uint8_t line = 0, i;
	uint8_t chr;
	for (uint8_t y = 0; y < 64; y += 8) {

		oled_setCursor(0, y);

		for (uint8_t parts = 0; parts < 8; parts++) {

			for (uint8_t column = 0; column < 16; column++) {
				chr = binArray[line * 128 + i];

				FRAME_BUFFER[disp_row][disp_column++] = chr;

				i++;
			}
		}
		i = 0;
		line++;
	}
}

void oled_resetLog()
{
	log_row = 0;
	log_scroll = 0;
}

// Print text with auto scroll
void oled_printLog(const struct device *dev, const char *log_msg)
{
	const sh1106_api *api = dev->api;
	oled_clearPart(dev, log_row >> 3, 0, 127);
	oled_printString(dev, log_msg, 0, log_row, 6, false);
	log_row += 16;
	if (log_row == 64) {
		log_row = 0;
		if (!log_scroll) {
			log_scroll = true;
		}
	}

	if (log_scroll) {
		api->set_line_address(dev, log_row);
	}
}

void oled_drawSine(const struct device *dev, float frequency, uint8_t shift, bool set)
{

	for (float i = 0; i < 128; i += 1) {

		oled_setPixel(dev, i, 10 * sin(2 * PI * frequency * i) + shift, set);
	}
}

long map(long x, long in_min, long in_max, long out_min, long out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void oled_setBattery(const struct device *dev, uint8_t percentage)
{
	uint8_t temp;
	temp = map(percentage, 0, 100, 2, 12);
	for (uint8_t i = 2; i <= temp; i += 2) {
		battery[i] = 0xFF;
	}

	for (uint8_t i = temp; i < 11; i++) {
		battery[i] = 0x81;
	}
	if (percentage == 100) {
		battery[12] = 0xFF;
	} else {
		battery[12] = 0xC3;
	}

	oled_setCursor(110, 0);

	for (uint8_t i = 0; i < 14; i++) {

		FRAME_BUFFER[disp_row][disp_column++] = battery[i];
	}
}

void oled_setSignal(const struct device *dev, uint8_t level)
{
	if (level == 0 || level < 20) {
		for (uint8_t i = 2; i <= 8; i += 2) {
			signal[i] = 0x00;
		}
	} else if (level >= 20 && level <= 50) {
		signal[2] = 0xC0;
		signal[4] = 0xF0;

		for (uint8_t i = 6; i <= 8; i += 2) {
			signal[i] = 0x00;
		}
	} else if (level > 50 && level <= 80) {
		signal[2] = 0xC0;
		signal[4] = 0xF0;
		signal[6] = 0xFC;
		signal[8] = 0x00;
	} else {
		signal[2] = 0xC0;
		signal[4] = 0xF0;
		signal[6] = 0xFC;
		signal[8] = 0xFF;
	}

	oled_setCursor(90, 0);

	for (uint8_t i = 0; i < 9; i++) {

		FRAME_BUFFER[disp_row][disp_column++] = signal[i];
	}
}

inline void oled_barInit()
{
	bar[0] = 0x3C;
	bar[1] = 0xC3;
	bar[126] = 0xC3;
	bar[127] = 0x3C;
}

void oled_setBar(const struct device *dev, uint8_t level, uint8_t page)
{
	oled_barInit();
	uint8_t temp = level;
	if (temp > 125) {
		temp = 0;
	}
	for (uint8_t i = 2; i <= temp; i++) {
		bar[i] = 0xBD;
	}

	for (uint8_t i = temp + 2; i <= 125; i++) {
		bar[i] = 0x81;
	}

	oled_setCursor(0, page * 8);
	for (uint8_t column = 0; column < 128; column++) {
		FRAME_BUFFER[disp_row][disp_column++] = bar[column];
	}
}
