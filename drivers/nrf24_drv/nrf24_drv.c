/*
 * Copyright (c) 2026 Surya Poudel
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nordic_nrf24

#include <errno.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <zephyr_oot/drivers/nrf24_drv.h>

LOG_MODULE_REGISTER(nrf24_driver);

#define NRF24_CMD_R_REGISTER 0x00
#define NRF24_CMD_W_REGISTER 0x20
#define NRF24_CMD_R_RX_PAYLOAD 0x61
#define NRF24_CMD_W_TX_PAYLOAD 0xA0
#define NRF24_CMD_FLUSH_TX 0xE1
#define NRF24_CMD_FLUSH_RX 0xE2
#define NRF24_CMD_R_RX_PL_WID 0x60
#define NRF24_CMD_W_TX_PAYLOAD_NO_ACK 0xB0
#define NRF24_CMD_NOP 0xFF

#define NRF24_REG_CONFIG 0x00
#define NRF24_REG_EN_AA 0x01
#define NRF24_REG_EN_RXADDR 0x02
#define NRF24_REG_SETUP_AW 0x03
#define NRF24_REG_SETUP_RETR 0x04
#define NRF24_REG_RF_CH 0x05
#define NRF24_REG_RF_SETUP 0x06
#define NRF24_REG_STATUS 0x07
#define NRF24_REG_RX_ADDR_P0 0x0A
#define NRF24_REG_TX_ADDR 0x10
#define NRF24_REG_RX_PW_P0 0x11
#define NRF24_REG_DYNPD 0x1C
#define NRF24_REG_FEATURE 0x1D

#define NRF24_CONFIG_PRIM_RX BIT(0)
#define NRF24_CONFIG_PWR_UP BIT(1)
#define NRF24_CONFIG_CRCO BIT(2)
#define NRF24_CONFIG_EN_CRC BIT(3)

#define NRF24_STATUS_MAX_RT BIT(4)
#define NRF24_STATUS_TX_DS BIT(5)
#define NRF24_STATUS_RX_DR BIT(6)

#define NRF24_RF_SETUP_PWR_MASK GENMASK(2, 1)
#define NRF24_RF_SETUP_DR_HIGH BIT(3)
#define NRF24_RF_SETUP_DR_LOW BIT(5)

#define NRF24_FEATURE_EN_DYN_ACK BIT(0)
#define NRF24_FEATURE_EN_DPL BIT(2)

#define NRF24_MAX_PIPE 5U
#define NRF24_POLL_TIMEOUT_MS 100

struct nrf24_register_cache {
	uint8_t config;
	uint8_t en_aa;
	uint8_t en_rxaddr;
	uint8_t setup_aw;
	uint8_t setup_retr;
	uint8_t rf_ch;
	uint8_t rf_setup;
	uint8_t dynpd;
	uint8_t feature;
	uint8_t payload_width[6];
};

struct nrf24_config {
	struct spi_dt_spec bus;
	struct gpio_dt_spec ce_gpio;
};

struct nrf24_data {
	struct k_mutex lock;
	struct nrf24_register_cache reg_cache;
};

static int nrf24_spi_transceive(const struct device *dev, const uint8_t *tx_buf,
				uint8_t *rx_buf, size_t len)
{
	const struct nrf24_config *config = dev->config;
	struct spi_buf tx = {
		.buf = (void *)tx_buf,
		.len = len,
	};
	struct spi_buf_set tx_set = {
		.buffers = &tx,
		.count = 1,
	};

	if (rx_buf != NULL) {
		struct spi_buf rx = {
			.buf = rx_buf,
			.len = len,
		};
		struct spi_buf_set rx_set = {
			.buffers = &rx,
			.count = 1,
		};

		return spi_transceive_dt(&config->bus, &tx_set, &rx_set);
	}

	return spi_write_dt(&config->bus, &tx_set);
}

static int nrf24_write_register(const struct device *dev, uint8_t reg_address,
				const uint8_t *reg_value, uint8_t len)
{
	uint8_t tx_buf[1 + 5] = {0};

	if (len > 5U) {
		return -EINVAL;
	}

	tx_buf[0] = NRF24_CMD_W_REGISTER | reg_address;
	memcpy(&tx_buf[1], reg_value, len);

	return nrf24_spi_transceive(dev, tx_buf, NULL, len + 1U);
}

static int nrf24_read_command(const struct device *dev, uint8_t cmd,
			      uint8_t *value)
{
	uint8_t tx_buf[2] = {cmd, NRF24_CMD_NOP};
	uint8_t rx_buf[2] = {0};
	int ret;

	ret = nrf24_spi_transceive(dev, tx_buf, rx_buf, sizeof(tx_buf));
	if (ret == 0) {
		*value = rx_buf[1];
	}

	return ret;
}

static int nrf24_write_command(const struct device *dev, uint8_t cmd)
{
	return nrf24_spi_transceive(dev, &cmd, NULL, 1U);
}

static int nrf24_read_status_locked(const struct device *dev, uint8_t *status)
{
	uint8_t tx_byte = NRF24_CMD_NOP;

	return nrf24_spi_transceive(dev, &tx_byte, status, 1U);
}

static uint8_t nrf24_address_width_bytes(const struct nrf24_data *data)
{
	switch (data->reg_cache.setup_aw & 0x03U) {
	case 0x01:
		return 3U;
	case 0x02:
		return 4U;
	case 0x03:
		return 5U;
	default:
		return 5U;
	}
}

static int nrf24_clear_irq_flags_locked(const struct device *dev, uint8_t flags)
{
	uint8_t value = flags & (NRF24_STATUS_RX_DR | NRF24_STATUS_TX_DS |
				 NRF24_STATUS_MAX_RT);

	return nrf24_write_register(dev, NRF24_REG_STATUS, &value, 1U);
}

static int nrf24_flush_tx_locked(const struct device *dev)
{
	return nrf24_write_command(dev, NRF24_CMD_FLUSH_TX);
}

static int nrf24_flush_rx_locked(const struct device *dev)
{
	return nrf24_write_command(dev, NRF24_CMD_FLUSH_RX);
}

static int nrf24_enable_crc_impl(const struct device *dev)
{
	struct nrf24_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	data->reg_cache.config |= NRF24_CONFIG_EN_CRC;
	ret = nrf24_write_register(dev, NRF24_REG_CONFIG,
				   &data->reg_cache.config, 1U);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_disable_crc_impl(const struct device *dev)
{
	struct nrf24_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	data->reg_cache.config &= ~NRF24_CONFIG_EN_CRC;
	ret = nrf24_write_register(dev, NRF24_REG_CONFIG,
				   &data->reg_cache.config, 1U);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_set_crc_encoding_impl(const struct device *dev, uint8_t byte_cnt)
{
	struct nrf24_data *data = dev->data;
	int ret;

	if ((byte_cnt == 0U) || (byte_cnt > 2U)) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	if (byte_cnt == 2U) {
		data->reg_cache.config |= NRF24_CONFIG_CRCO;
	} else {
		data->reg_cache.config &= ~NRF24_CONFIG_CRCO;
	}

	ret = nrf24_write_register(dev, NRF24_REG_CONFIG,
				   &data->reg_cache.config, 1U);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_set_address_width_impl(const struct device *dev, uint8_t width)
{
	struct nrf24_data *data = dev->data;
	uint8_t value;
	int ret;

	if ((width < 3U) || (width > 5U)) {
		return -EINVAL;
	}

	value = width - 2U;

	k_mutex_lock(&data->lock, K_FOREVER);
	data->reg_cache.setup_aw = value;
	ret = nrf24_write_register(dev, NRF24_REG_SETUP_AW,
				   &data->reg_cache.setup_aw, 1U);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_set_channel_impl(const struct device *dev, uint8_t channel)
{
	struct nrf24_data *data = dev->data;
	int ret;

	if (channel > 125U) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	data->reg_cache.rf_ch = channel;
	ret = nrf24_write_register(dev, NRF24_REG_RF_CH,
				   &data->reg_cache.rf_ch, 1U);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_set_data_rate_impl(const struct device *dev,
				    nrf24_data_rate_t data_rate)
{
	struct nrf24_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	data->reg_cache.rf_setup &=
		~(NRF24_RF_SETUP_DR_HIGH | NRF24_RF_SETUP_DR_LOW);

	switch (data_rate) {
	case DR_1MBPS:
		break;
	case DR_2MBPS:
		data->reg_cache.rf_setup |= NRF24_RF_SETUP_DR_HIGH;
		break;
	case DR_250KBPS:
		data->reg_cache.rf_setup |= NRF24_RF_SETUP_DR_LOW;
		break;
	default:
		k_mutex_unlock(&data->lock);
		return -EINVAL;
	}

	ret = nrf24_write_register(dev, NRF24_REG_RF_SETUP,
				   &data->reg_cache.rf_setup, 1U);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_set_tx_power_impl(const struct device *dev,
				   nrf24_tx_power_t power_level)
{
	struct nrf24_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	data->reg_cache.rf_setup &= ~NRF24_RF_SETUP_PWR_MASK;

	switch (power_level) {
	case TX_POWER_MAX:
		data->reg_cache.rf_setup |= 0x06U;
		break;
	case TX_POWER_MODERATE:
		data->reg_cache.rf_setup |= 0x04U;
		break;
	case TX_POWER_MIN:
		break;
	default:
		k_mutex_unlock(&data->lock);
		return -EINVAL;
	}

	ret = nrf24_write_register(dev, NRF24_REG_RF_SETUP,
				   &data->reg_cache.rf_setup, 1U);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_enable_rx_pipe_impl(const struct device *dev, uint8_t pipe)
{
	struct nrf24_data *data = dev->data;
	int ret;

	if (pipe > NRF24_MAX_PIPE) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	data->reg_cache.en_rxaddr |= BIT(pipe);
	ret = nrf24_write_register(dev, NRF24_REG_EN_RXADDR,
				   &data->reg_cache.en_rxaddr, 1U);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_set_rx_address_impl(const struct device *dev, uint8_t pipe,
				     const uint8_t *address)
{
	struct nrf24_data *data = dev->data;
	uint8_t addr_len;
	int ret;

	if ((address == NULL) || (pipe > NRF24_MAX_PIPE)) {
		return -EINVAL;
	}

	ret = nrf24_enable_rx_pipe_impl(dev, pipe);
	if (ret != 0) {
		return ret;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	addr_len = nrf24_address_width_bytes(data);
	if (pipe < 2U) {
		ret = nrf24_write_register(dev, NRF24_REG_RX_ADDR_P0 + pipe,
					   address, addr_len);
	} else {
		ret = nrf24_write_register(dev, NRF24_REG_RX_ADDR_P0 + pipe,
					   address, 1U);
	}
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_set_tx_address_impl(const struct device *dev,
				     const uint8_t *address)
{
	struct nrf24_data *data = dev->data;
	uint8_t addr_len;
	int ret;

	if (address == NULL) {
		return -EINVAL;
	}

	ret = nrf24_set_rx_address_impl(dev, 0U, address);
	if (ret != 0) {
		return ret;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	addr_len = nrf24_address_width_bytes(data);
	ret = nrf24_write_register(dev, NRF24_REG_TX_ADDR, address, addr_len);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_set_payload_size_impl(const struct device *dev, uint8_t size)
{
	struct nrf24_data *data = dev->data;
	int ret = 0;
	uint8_t pipe;

	if (size > NRF24_MAX_PAYLOAD_SIZE) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	for (pipe = 0U; pipe <= NRF24_MAX_PIPE; pipe++) {
		data->reg_cache.payload_width[pipe] = size;
		ret = nrf24_write_register(dev, NRF24_REG_RX_PW_P0 + pipe,
					   &data->reg_cache.payload_width[pipe],
					   1U);
		if (ret != 0) {
			break;
		}
	}
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_set_arc_count_impl(const struct device *dev, uint8_t count)
{
	struct nrf24_data *data = dev->data;
	int ret;

	if (count > 15U) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	data->reg_cache.setup_retr &= 0xF0U;
	data->reg_cache.setup_retr |= count;
	ret = nrf24_write_register(dev, NRF24_REG_SETUP_RETR,
				   &data->reg_cache.setup_retr, 1U);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_set_retransmit_delay_impl(const struct device *dev,
					   uint8_t delay)
{
	struct nrf24_data *data = dev->data;
	int ret;

	if (delay > 15U) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	data->reg_cache.setup_retr &= 0x0FU;
	data->reg_cache.setup_retr |= delay << 4;
	ret = nrf24_write_register(dev, NRF24_REG_SETUP_RETR,
				   &data->reg_cache.setup_retr, 1U);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_set_auto_ack_impl(const struct device *dev, bool set_value)
{
	struct nrf24_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	data->reg_cache.en_aa = set_value ? 0x3FU : 0x00U;
	ret = nrf24_write_register(dev, NRF24_REG_EN_AA,
				   &data->reg_cache.en_aa, 1U);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_enable_dynamic_payload_length_impl(const struct device *dev)
{
	struct nrf24_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	data->reg_cache.feature |= NRF24_FEATURE_EN_DPL;
	ret = nrf24_write_register(dev, NRF24_REG_FEATURE,
				   &data->reg_cache.feature, 1U);
	if (ret == 0) {
		data->reg_cache.dynpd = 0x3FU;
		ret = nrf24_write_register(dev, NRF24_REG_DYNPD,
					   &data->reg_cache.dynpd, 1U);
	}
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_disable_dynamic_payload_length_impl(const struct device *dev)
{
	struct nrf24_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	data->reg_cache.feature &= ~NRF24_FEATURE_EN_DPL;
	ret = nrf24_write_register(dev, NRF24_REG_FEATURE,
				   &data->reg_cache.feature, 1U);
	if (ret == 0) {
		data->reg_cache.dynpd = 0x00U;
		ret = nrf24_write_register(dev, NRF24_REG_DYNPD,
					   &data->reg_cache.dynpd, 1U);
	}
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_enable_dynamic_ack_impl(const struct device *dev)
{
	struct nrf24_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	data->reg_cache.feature |= NRF24_FEATURE_EN_DYN_ACK;
	ret = nrf24_write_register(dev, NRF24_REG_FEATURE,
				   &data->reg_cache.feature, 1U);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_set_mode_impl(const struct device *dev, radio_mode_t mode)
{
	const struct nrf24_config *config = dev->config;
	struct nrf24_data *data = dev->data;
	int ret = 0;

	k_mutex_lock(&data->lock, K_FOREVER);
	switch (mode) {
	case TX_MODE:
		ret = gpio_pin_set_dt(&config->ce_gpio, 0);
		if (ret == 0) {
			data->reg_cache.config &= ~NRF24_CONFIG_PRIM_RX;
			ret = nrf24_write_register(dev, NRF24_REG_CONFIG,
						   &data->reg_cache.config, 1U);
		}
		break;
	case RX_MODE:
		data->reg_cache.config |= NRF24_CONFIG_PRIM_RX;
		ret = nrf24_write_register(dev, NRF24_REG_CONFIG,
					   &data->reg_cache.config, 1U);
		if (ret == 0) {
			ret = gpio_pin_set_dt(&config->ce_gpio, 1);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_power_up_impl(const struct device *dev)
{
	struct nrf24_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	data->reg_cache.config |= NRF24_CONFIG_PWR_UP;
	ret = nrf24_write_register(dev, NRF24_REG_CONFIG,
				   &data->reg_cache.config, 1U);
	k_mutex_unlock(&data->lock);

	if (ret == 0) {
		k_sleep(K_MSEC(2));
	}

	return ret;
}

static int nrf24_flush_tx_impl(const struct device *dev)
{
	struct nrf24_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = nrf24_flush_tx_locked(dev);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_flush_rx_impl(const struct device *dev)
{
	struct nrf24_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = nrf24_flush_rx_locked(dev);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_clear_irq_flags_impl(const struct device *dev, uint8_t flags)
{
	struct nrf24_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = nrf24_clear_irq_flags_locked(dev, flags);
	k_mutex_unlock(&data->lock);

	return ret;
}

static uint8_t nrf24_read_status_impl(const struct device *dev)
{
	struct nrf24_data *data = dev->data;
	uint8_t status = 0xFFU;

	k_mutex_lock(&data->lock, K_FOREVER);
	if (nrf24_read_status_locked(dev, &status) != 0) {
		status = 0xFFU;
	}
	k_mutex_unlock(&data->lock);

	return status;
}

static bool nrf24_available_impl(const struct device *dev, uint8_t *pipe)
{
	struct nrf24_data *data = dev->data;
	uint8_t status = 0U;
	uint8_t rx_pipe;
	bool available = false;

	k_mutex_lock(&data->lock, K_FOREVER);
	if (nrf24_read_status_locked(dev, &status) == 0) {
		if ((status & NRF24_STATUS_RX_DR) != 0U) {
			rx_pipe = (status >> 1) & 0x07U;
			if (rx_pipe <= NRF24_MAX_PIPE) {
				if (pipe != NULL) {
					*pipe = rx_pipe;
				}
				available = true;
			}
		}
	}
	k_mutex_unlock(&data->lock);

	return available;
}

static int nrf24_tx_common(const struct device *dev, const uint8_t *buffer,
			   uint8_t length, bool no_ack)
{
	const struct nrf24_config *config = dev->config;
	struct nrf24_data *data = dev->data;
	uint8_t tx_buffer[NRF24_MAX_PAYLOAD_SIZE + 1] = {0};
	uint8_t status = 0U;
	int64_t deadline;
	int ret;

	if ((buffer == NULL) || (length > NRF24_MAX_PAYLOAD_SIZE)) {
		return -EINVAL;
	}

	tx_buffer[0] = no_ack ? NRF24_CMD_W_TX_PAYLOAD_NO_ACK
			      : NRF24_CMD_W_TX_PAYLOAD;
	memcpy(&tx_buffer[1], buffer, length);

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = nrf24_spi_transceive(dev, tx_buffer, NULL, length + 1U);
	if (ret != 0) {
		k_mutex_unlock(&data->lock);
		return ret;
	}

	ret = gpio_pin_set_dt(&config->ce_gpio, 1);
	if (ret == 0) {
		k_busy_wait(10);
		ret = gpio_pin_set_dt(&config->ce_gpio, 0);
	}
	if (ret != 0) {
		k_mutex_unlock(&data->lock);
		return ret;
	}

	deadline = k_uptime_get() + NRF24_POLL_TIMEOUT_MS;
	do {
		ret = nrf24_read_status_locked(dev, &status);
		if (ret != 0) {
			k_mutex_unlock(&data->lock);
			return ret;
		}

		if ((status & NRF24_STATUS_MAX_RT) != 0U) {
			(void)nrf24_clear_irq_flags_locked(
				dev, NRF24_STATUS_TX_DS | NRF24_STATUS_RX_DR |
					     NRF24_STATUS_MAX_RT);
			(void)nrf24_flush_tx_locked(dev);
			(void)nrf24_flush_rx_locked(dev);
			k_mutex_unlock(&data->lock);
			return -EIO;
		}

		if ((status & NRF24_STATUS_TX_DS) != 0U) {
			ret = nrf24_clear_irq_flags_locked(
				dev, NRF24_STATUS_TX_DS | NRF24_STATUS_RX_DR |
					     NRF24_STATUS_MAX_RT);
			k_mutex_unlock(&data->lock);
			return ret;
		}

		k_sleep(K_USEC(50));
	} while (k_uptime_get() < deadline);

	k_mutex_unlock(&data->lock);

	return -ETIMEDOUT;
}

static int nrf24_tx_impl(const struct device *dev, const uint8_t *buffer,
			 uint8_t length)
{
	return nrf24_tx_common(dev, buffer, length, false);
}

static int nrf24_tx_no_ack_impl(const struct device *dev, const uint8_t *buffer,
				uint8_t length)
{
	return nrf24_tx_common(dev, buffer, length, true);
}

static int nrf24_rx_impl(const struct device *dev, uint8_t *buffer,
			 uint8_t length)
{
	struct nrf24_data *data = dev->data;
	uint8_t tx_buffer[NRF24_MAX_PAYLOAD_SIZE + 1] = {0};
	uint8_t rx_buffer[NRF24_MAX_PAYLOAD_SIZE + 1] = {0};
	int ret;

	if ((buffer == NULL) || (length > NRF24_MAX_PAYLOAD_SIZE)) {
		return -EINVAL;
	}

	tx_buffer[0] = NRF24_CMD_R_RX_PAYLOAD;

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = nrf24_spi_transceive(dev, tx_buffer, rx_buffer, length + 1U);
	if (ret == 0) {
		memcpy(buffer, &rx_buffer[1], length);
		ret = nrf24_clear_irq_flags_locked(dev, NRF24_STATUS_RX_DR);
		if (ret == 0) {
			ret = nrf24_flush_rx_locked(dev);
		}
		if (ret == 0) {
			ret = nrf24_flush_tx_locked(dev);
		}
	}
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_get_rx_payload_size_impl(const struct device *dev,
					  uint8_t *size)
{
	struct nrf24_data *data = dev->data;
	int ret;
	uint8_t width = 0U;

	if (size == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = nrf24_read_command(dev, NRF24_CMD_R_RX_PL_WID, &width);
	if ((ret == 0) && (width > NRF24_MAX_PAYLOAD_SIZE)) {
		(void)nrf24_flush_rx_locked(dev);
		ret = -EIO;
	}
	if (ret == 0) {
		*size = width;
	}
	k_mutex_unlock(&data->lock);

	return ret;
}

static int nrf24_apply_default_config(const struct device *dev)
{
	int ret;

	ret = nrf24_disable_dynamic_payload_length(dev);
	if (ret != 0) {
		return ret;
	}

	ret = nrf24_enable_crc(dev);
	if (ret != 0) {
		return ret;
	}

	ret = nrf24_set_crc_encoding(dev, 2U);
	if (ret != 0) {
		return ret;
	}

	ret = nrf24_set_address_width(dev, 5U);
	if (ret != 0) {
		return ret;
	}

	ret = nrf24_set_payload_size(dev, NRF24_MAX_PAYLOAD_SIZE);
	if (ret != 0) {
		return ret;
	}

	ret = nrf24_set_arc_count(dev, 0U);
	if (ret != 0) {
		return ret;
	}

	ret = nrf24_set_auto_ack(dev, false);
	if (ret != 0) {
		return ret;
	}

	ret = nrf24_set_retransmit_delay(dev, 0U);
	if (ret != 0) {
		return ret;
	}

	ret = nrf24_set_data_rate(dev, DR_1MBPS);
	if (ret != 0) {
		return ret;
	}

	ret = nrf24_enable_rx_pipe(dev, 0U);
	if (ret != 0) {
		return ret;
	}

	ret = nrf24_enable_rx_pipe(dev, 1U);
	if (ret != 0) {
		return ret;
	}

	ret = nrf24_set_tx_power(dev, TX_POWER_MAX);
	if (ret != 0) {
		return ret;
	}

	ret = nrf24_flush_tx(dev);
	if (ret != 0) {
		return ret;
	}

	ret = nrf24_flush_rx(dev);
	if (ret != 0) {
		return ret;
	}

	ret = nrf24_clear_irq_flags(
		dev, NRF24_STATUS_TX_DS | NRF24_STATUS_RX_DR |
			     NRF24_STATUS_MAX_RT);
	if (ret != 0) {
		return ret;
	}

	return nrf24_power_up(dev);
}

static int nrf24_init(const struct device *dev)
{
	const struct nrf24_config *config = dev->config;
	struct nrf24_data *data = dev->data;
	int ret;

	if (!spi_is_ready_dt(&config->bus)) {
		LOG_ERR("SPI bus is not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&config->ce_gpio)) {
		LOG_ERR("CE gpio is not ready");
		return -ENODEV;
	}

	k_mutex_init(&data->lock);
	memset(&data->reg_cache, 0, sizeof(data->reg_cache));

	ret = gpio_pin_configure_dt(&config->ce_gpio, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		LOG_ERR("Failed to configure CE pin: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(5));

	ret = nrf24_apply_default_config(dev);
	if (ret != 0) {
		LOG_ERR("Failed to apply default config: %d", ret);
	}

	return ret;
}

static const nrf24_api api = {
	.enable_crc = nrf24_enable_crc_impl,
	.disable_crc = nrf24_disable_crc_impl,
	.set_tx_address = nrf24_set_tx_address_impl,
	.set_rx_address = nrf24_set_rx_address_impl,
	.set_tx_power = nrf24_set_tx_power_impl,
	.set_data_rate = nrf24_set_data_rate_impl,
	.set_channel = nrf24_set_channel_impl,
	.set_address_width = nrf24_set_address_width_impl,
	.set_mode = nrf24_set_mode_impl,
	.available = nrf24_available_impl,
	.rx = nrf24_rx_impl,
	.tx = nrf24_tx_impl,
	.tx_no_ack = nrf24_tx_no_ack_impl,
	.set_payload_size = nrf24_set_payload_size_impl,
	.clear_irq_flags = nrf24_clear_irq_flags_impl,
	.set_arc_count = nrf24_set_arc_count_impl,
	.set_retransmit_delay = nrf24_set_retransmit_delay_impl,
	.enable_rx_pipe = nrf24_enable_rx_pipe_impl,
	.get_rx_payload_size = nrf24_get_rx_payload_size_impl,
	.power_up = nrf24_power_up_impl,
	.set_auto_ack = nrf24_set_auto_ack_impl,
	.flush_tx = nrf24_flush_tx_impl,
	.flush_rx = nrf24_flush_rx_impl,
	.enable_dynamic_ack = nrf24_enable_dynamic_ack_impl,
	.enable_dynamic_payload_length =
		nrf24_enable_dynamic_payload_length_impl,
	.disable_dynamic_payload_length =
		nrf24_disable_dynamic_payload_length_impl,
	.set_crc_encoding = nrf24_set_crc_encoding_impl,
	.read_status = nrf24_read_status_impl,
};

#define NRF24_DEFINE(inst)                                                     \
	static struct nrf24_data nrf24_data_##inst;                           \
	static const struct nrf24_config nrf24_config_##inst = {              \
		.bus = SPI_DT_SPEC_INST_GET(                                   \
			inst, SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB |          \
				      SPI_WORD_SET(8)),                     \
		.ce_gpio = GPIO_DT_SPEC_INST_GET(inst, ce_gpios),            \
	};                                                                  \
	DEVICE_DT_INST_DEFINE(inst, nrf24_init, NULL, &nrf24_data_##inst,   \
			      &nrf24_config_##inst, POST_KERNEL,              \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &api);

DT_INST_FOREACH_STATUS_OKAY(NRF24_DEFINE)
