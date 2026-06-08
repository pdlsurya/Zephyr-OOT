/*
 * Copyright (c) 2026 Surya Poudel
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_NRF24_DRV_H_
#define ZEPHYR_DRIVERS_NRF24_DRV_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NRF24_MAX_PAYLOAD_SIZE 32U

typedef enum {
	DR_1MBPS,
	DR_2MBPS,
	DR_250KBPS
} nrf24_data_rate_t;

typedef enum {
	TX_POWER_MAX,
	TX_POWER_MODERATE,
	TX_POWER_MIN
} nrf24_tx_power_t;

typedef enum {
	TX_MODE,
	RX_MODE
} radio_mode_t;

typedef struct {
	int (*enable_crc)(const struct device *dev);
	int (*disable_crc)(const struct device *dev);
	int (*set_tx_address)(const struct device *dev, const uint8_t *address);
	int (*set_rx_address)(const struct device *dev, uint8_t pipe,
			      const uint8_t *address);
	int (*set_tx_power)(const struct device *dev,
			    nrf24_tx_power_t power_level);
	int (*set_data_rate)(const struct device *dev,
			     nrf24_data_rate_t data_rate);
	int (*set_channel)(const struct device *dev, uint8_t channel);
	int (*set_address_width)(const struct device *dev, uint8_t width);
	int (*set_mode)(const struct device *dev, radio_mode_t mode);
	bool (*available)(const struct device *dev, uint8_t *pipe);
	int (*rx)(const struct device *dev, uint8_t *buffer, uint8_t length);
	int (*tx)(const struct device *dev, const uint8_t *buffer,
		  uint8_t length);
	int (*tx_no_ack)(const struct device *dev, const uint8_t *buffer,
			 uint8_t length);
	int (*set_payload_size)(const struct device *dev, uint8_t size);
	int (*clear_irq_flags)(const struct device *dev, uint8_t flags);
	int (*set_arc_count)(const struct device *dev, uint8_t count);
	int (*set_retransmit_delay)(const struct device *dev, uint8_t delay);
	int (*enable_rx_pipe)(const struct device *dev, uint8_t pipe);
	int (*get_rx_payload_size)(const struct device *dev, uint8_t *size);
	int (*power_up)(const struct device *dev);
	int (*set_auto_ack)(const struct device *dev, bool set_value);
	int (*flush_tx)(const struct device *dev);
	int (*flush_rx)(const struct device *dev);
	int (*enable_dynamic_ack)(const struct device *dev);
	int (*enable_dynamic_payload_length)(const struct device *dev);
	int (*disable_dynamic_payload_length)(const struct device *dev);
	int (*set_crc_encoding)(const struct device *dev, uint8_t byte_cnt);
	uint8_t (*read_status)(const struct device *dev);
} nrf24_api;

static inline int nrf24_enable_crc(const struct device *dev)
{
	const nrf24_api *api = dev->api;

	return api->enable_crc(dev);
}

static inline int nrf24_disable_crc(const struct device *dev)
{
	const nrf24_api *api = dev->api;

	return api->disable_crc(dev);
}

static inline int nrf24_set_tx_address(const struct device *dev,
				       const uint8_t *address)
{
	const nrf24_api *api = dev->api;

	return api->set_tx_address(dev, address);
}

static inline int nrf24_set_rx_address(const struct device *dev, uint8_t pipe,
				       const uint8_t *address)
{
	const nrf24_api *api = dev->api;

	return api->set_rx_address(dev, pipe, address);
}

static inline int nrf24_set_tx_power(const struct device *dev,
				     nrf24_tx_power_t power_level)
{
	const nrf24_api *api = dev->api;

	return api->set_tx_power(dev, power_level);
}

static inline int nrf24_set_data_rate(const struct device *dev,
				      nrf24_data_rate_t data_rate)
{
	const nrf24_api *api = dev->api;

	return api->set_data_rate(dev, data_rate);
}

static inline int nrf24_set_channel(const struct device *dev, uint8_t channel)
{
	const nrf24_api *api = dev->api;

	return api->set_channel(dev, channel);
}

static inline int nrf24_set_address_width(const struct device *dev,
					  uint8_t width)
{
	const nrf24_api *api = dev->api;

	return api->set_address_width(dev, width);
}

static inline int nrf24_set_mode(const struct device *dev, radio_mode_t mode)
{
	const nrf24_api *api = dev->api;

	return api->set_mode(dev, mode);
}

static inline bool nrf24_available(const struct device *dev, uint8_t *pipe)
{
	const nrf24_api *api = dev->api;

	return api->available(dev, pipe);
}

static inline int nrf24_rx(const struct device *dev, uint8_t *buffer,
			   uint8_t length)
{
	const nrf24_api *api = dev->api;

	return api->rx(dev, buffer, length);
}

static inline int nrf24_tx(const struct device *dev, const uint8_t *buffer,
			   uint8_t length)
{
	const nrf24_api *api = dev->api;

	return api->tx(dev, buffer, length);
}

static inline int nrf24_tx_no_ack(const struct device *dev,
				  const uint8_t *buffer, uint8_t length)
{
	const nrf24_api *api = dev->api;

	return api->tx_no_ack(dev, buffer, length);
}

static inline int nrf24_set_payload_size(const struct device *dev, uint8_t size)
{
	const nrf24_api *api = dev->api;

	return api->set_payload_size(dev, size);
}

static inline int nrf24_clear_irq_flags(const struct device *dev, uint8_t flags)
{
	const nrf24_api *api = dev->api;

	return api->clear_irq_flags(dev, flags);
}

static inline int nrf24_set_arc_count(const struct device *dev, uint8_t count)
{
	const nrf24_api *api = dev->api;

	return api->set_arc_count(dev, count);
}

static inline int nrf24_set_retransmit_delay(const struct device *dev,
					     uint8_t delay)
{
	const nrf24_api *api = dev->api;

	return api->set_retransmit_delay(dev, delay);
}

static inline int nrf24_enable_rx_pipe(const struct device *dev, uint8_t pipe)
{
	const nrf24_api *api = dev->api;

	return api->enable_rx_pipe(dev, pipe);
}

static inline int nrf24_get_rx_payload_size(const struct device *dev,
					    uint8_t *size)
{
	const nrf24_api *api = dev->api;

	return api->get_rx_payload_size(dev, size);
}

static inline int nrf24_power_up(const struct device *dev)
{
	const nrf24_api *api = dev->api;

	return api->power_up(dev);
}

static inline int nrf24_set_auto_ack(const struct device *dev, bool set_value)
{
	const nrf24_api *api = dev->api;

	return api->set_auto_ack(dev, set_value);
}

static inline int nrf24_flush_tx(const struct device *dev)
{
	const nrf24_api *api = dev->api;

	return api->flush_tx(dev);
}

static inline int nrf24_flush_rx(const struct device *dev)
{
	const nrf24_api *api = dev->api;

	return api->flush_rx(dev);
}

static inline int nrf24_enable_dynamic_ack(const struct device *dev)
{
	const nrf24_api *api = dev->api;

	return api->enable_dynamic_ack(dev);
}

static inline int nrf24_enable_dynamic_payload_length(const struct device *dev)
{
	const nrf24_api *api = dev->api;

	return api->enable_dynamic_payload_length(dev);
}

static inline int nrf24_disable_dynamic_payload_length(const struct device *dev)
{
	const nrf24_api *api = dev->api;

	return api->disable_dynamic_payload_length(dev);
}

static inline int nrf24_set_crc_encoding(const struct device *dev,
					 uint8_t byte_cnt)
{
	const nrf24_api *api = dev->api;

	return api->set_crc_encoding(dev, byte_cnt);
}

static inline uint8_t nrf24_read_status(const struct device *dev)
{
	const nrf24_api *api = dev->api;

	return api->read_status(dev);
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_NRF24_DRV_H_ */
