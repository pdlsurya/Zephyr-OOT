/*
 * Copyright (c) 2023 Surya Poudel
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SH1106_DRIVER_H
#define __SH1106_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>

typedef struct {
	struct i2c_dt_spec bus;
	uint8_t i2c_cfg;

}sh1106_config_t;

typedef struct{
 void (*display_on)(const struct device *dev);
 void (*display_off)(const struct device *dev);
 void (*set_contrast)(const struct device *dev, uint8_t constrast_level);
 void (*write_buffer)(const struct device *dev);
 void (*update_page)(const struct device *dev, uint8_t page);
 void (*set_line_address)(const struct device *dev,uint8_t line_address);
}sh1106_api;



#endif

