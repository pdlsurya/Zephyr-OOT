/*
 * Copyright (c) 2026 Surya Poudel
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SD_DRV_H_
#define ZEPHYR_DRIVERS_SD_DRV_H_

#include <stdint.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	SD_READY,
	SD_INIT_SUCCESS,
	SD_INIT_ERROR,
	SD_READ_SUCCESS,
	SD_READ_ERROR,
	SD_WRITE_SUCCESS,
	SD_WRITE_ERROR
} sd_ret_t;

typedef struct {
	sd_ret_t (*card_init)(const struct device *dev);
	sd_ret_t (*read_sector)(const struct device *dev, uint32_t sector,
				uint8_t *buf);
	sd_ret_t (*write_sector)(const struct device *dev, uint32_t sector,
				 const uint8_t *buf);
	uint8_t (*read_multiple_start)(const struct device *dev,
				       uint32_t start_addr);
	sd_ret_t (*read_multiple)(const struct device *dev, uint8_t *buf);
	void (*read_multiple_stop)(const struct device *dev);
} sd_drv_api_t;

static inline sd_ret_t sd_drv_card_init(const struct device *dev)
{
	const sd_drv_api_t *api = dev->api;

	return api->card_init(dev);
}

static inline sd_ret_t sd_drv_read_sector(const struct device *dev,
					  uint32_t sector, uint8_t *buf)
{
	const sd_drv_api_t *api = dev->api;

	return api->read_sector(dev, sector, buf);
}

static inline sd_ret_t sd_drv_write_sector(const struct device *dev,
					   uint32_t sector,
					   const uint8_t *buf)
{
	const sd_drv_api_t *api = dev->api;

	return api->write_sector(dev, sector, buf);
}

static inline uint8_t sd_drv_read_multiple_start(const struct device *dev,
						 uint32_t start_addr)
{
	const sd_drv_api_t *api = dev->api;

	return api->read_multiple_start(dev, start_addr);
}

static inline sd_ret_t sd_drv_read_multiple(const struct device *dev,
					    uint8_t *buf)
{
	const sd_drv_api_t *api = dev->api;

	return api->read_multiple(dev, buf);
}

static inline void sd_drv_read_multiple_stop(const struct device *dev)
{
	const sd_drv_api_t *api = dev->api;

	api->read_multiple_stop(dev);
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_SD_DRV_H_ */
