#ifndef __OLED_SH1106_H
#define __OLED_SH1106_H

#include<zephyr/device.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr_oot/drivers/sh1106_drv.h>


void oled_init();

void oled_displayOn(const struct device *dev);

void oled_clearDisplay();

void oled_clearPart(const struct device *dev,uint8_t page, uint8_t start_pos, uint8_t end_pos);

void oled_setCursor(uint8_t x, uint8_t y);

void oled_displayBmp(const struct device *dev,const uint8_t* data);

void oled_printChar(const struct device *dev,char C, uint8_t xpos, uint8_t ypos, uint8_t font_size, bool Highlight);

void oled_printString(const struct device *dev,const char *str, uint8_t xpos, uint8_t ypos, uint8_t font_size, bool Highlight);

void oled_setBattery(const struct device *dev,uint8_t percentage);

void oled_setSignal(const struct device *dev,uint8_t level);

void oled_setBar(const struct device *dev,uint8_t level, uint8_t page);

void oled_display(const struct device *dev);

void oled_print7Seg_number(const struct device *dev,const char *str, uint8_t xpos, uint8_t ypos);

void oled_print7Seg_digit(const struct device *dev,char C, uint8_t xpos, uint8_t ypos);

void oled_writeByte(const struct device *dev,uint8_t byte);

void oled_setPixel(const struct device *dev,uint8_t x, uint8_t y, bool set);

void oled_drawLine(const struct device *dev,float x1, float y1, float x2, float y2, bool set);

void oled_drawRectangle(const struct device *dev,uint8_t x1,uint8_t y1, uint8_t x2, uint8_t y2, bool set);

void oled_drawCircle(const struct device *dev,float Cx, float Cy, float radius, bool set);

void oled_setDisplayStart(const struct device *dev,uint8_t Start);

void oled_setLineAddress(const struct device *dev,uint8_t address);

void oled_printLog(const struct device *dev,const char *log_msg);

void oled_drawSine(const struct device *dev,float frequency, uint8_t shift, bool set);

void oled_plot(const struct device *dev,float x, float y);

void oled_setContrast(const struct device *dev,uint8_t level);

void oled_resetLog();

long map(long x, long in_min, long in_max, long out_min, long out_max);

#endif
