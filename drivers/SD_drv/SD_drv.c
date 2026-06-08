/*
 * Copyright (c) 2026 Surya Poudel
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_sdhc_spi_slot

#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr_oot/drivers/SD_drv.h>

LOG_MODULE_REGISTER(sd_drv);

#define CMD0 0U
#define CMD0_ARG 0x00000000U
#define CMD0_CRC 0x94U

#define CMD8 8U
#define CMD8_ARG 0x000001AAU
#define CMD8_CRC 0x86U

#define CMD9 9U
#define CMD9_ARG 0x00000000U
#define CMD9_CRC 0x00U

#define CMD58 58U
#define CMD58_ARG 0x00000000U
#define CMD58_CRC 0x00U

#define CMD55 55U
#define CMD55_ARG 0x00000000U
#define CMD55_CRC 0x00U

#define ACMD41 41U
#define ACMD41_ARG 0x40000000U
#define ACMD41_CRC 0x00U

#define CMD17 17U
#define CMD17_CRC 0x95U

#define CMD24 24U
#define CMD24_CRC 0x00U

#define CMD18 18U
#define CMD18_CRC 0x00U

#define CMD12 12U
#define CMD12_ARG 0x00000000U
#define CMD12_CRC 0x00U

#define SD_MAX_READ_ATTEMPTS 4500U
#define SD_MAX_WRITE_ATTEMPTS 3907U

#define PARAM_ERROR(x) ((x) & 0b01000000U)
#define ADDR_ERROR(x) ((x) & 0b00100000U)
#define ERASE_SEQ_ERROR(x) ((x) & 0b00010000U)
#define CRC_ERROR(x) ((x) & 0b00001000U)
#define ILLEGAL_CMD(x) ((x) & 0b00000100U)
#define ERASE_RESET(x) ((x) & 0b00000010U)
#define IN_IDLE(x) ((x) & 0b00000001U)

#define VOL_ACC(x) ((x) & 0x1FU)

#define VOLTAGE_ACC_27_33 0b00000001U
#define VOLTAGE_ACC_LOW 0b00000010U
#define VOLTAGE_ACC_RES1 0b00000100U
#define VOLTAGE_ACC_RES2 0b00001000U

#define POWER_UP_STATUS(x) ((x) & 0x80U)
#define CCS_VAL(x) ((x) & 0x40U)
#define VDD_2728(x) ((x) & 0b10000000U)
#define VDD_2829(x) ((x) & 0b00000001U)
#define VDD_2930(x) ((x) & 0b00000010U)
#define VDD_3031(x) ((x) & 0b00000100U)
#define VDD_3132(x) ((x) & 0b00001000U)
#define VDD_3233(x) ((x) & 0b00010000U)
#define VDD_3334(x) ((x) & 0b00100000U)
#define VDD_3435(x) ((x) & 0b01000000U)
#define VDD_3536(x) ((x) & 0b10000000U)

#define SD_TOKEN_OOR(x) ((x) & 0b00001000U)
#define SD_TOKEN_CECC(x) ((x) & 0b00000100U)
#define SD_TOKEN_CC(x) ((x) & 0b00000010U)
#define SD_TOKEN_ERROR(x) ((x) & 0b00000001U)

#define SD_START_TOKEN 0xFEU
#define SD_BLOCK_LEN 512U

struct sd_drv_config {
	struct spi_dt_spec bus;
};

struct sd_drv_data {
	struct k_mutex lock;
	uint8_t spi_tx_buf[SD_BLOCK_LEN];
	bool initialized;
	bool multi_read_active;
};

static int sd_spi_transceive(const struct device *dev, const uint8_t *tx_buffer,
			     uint8_t *rx_buffer, uint16_t len)
{
	const struct sd_drv_config *config = dev->config;
	struct spi_buf tx_buf = {
		.buf = (void *)tx_buffer,
		.len = len,
	};
	struct spi_buf_set tx_bufs = {
		.buffers = &tx_buf,
		.count = 1,
	};

	if (rx_buffer != NULL) {
		struct spi_buf rx_buf = {
			.buf = rx_buffer,
			.len = len,
		};
		struct spi_buf_set rx_bufs = {
			.buffers = &rx_buf,
			.count = 1,
		};

		return spi_transceive_dt(&config->bus, &tx_bufs, &rx_bufs);
	}

	return spi_write_dt(&config->bus, &tx_bufs);
}

static void sd_release_bus(const struct device *dev)
{
	const struct sd_drv_config *config = dev->config;
	struct sd_drv_data *data = dev->data;

	(void)sd_spi_transceive(dev, data->spi_tx_buf, NULL, 1U);
	(void)spi_release_dt(&config->bus);
}

static void sd_power_up_seq_locked(const struct device *dev)
{
	const struct sd_drv_config *config = dev->config;
	struct sd_drv_data *data = dev->data;

	(void)spi_release_dt(&config->bus);
	memset(data->spi_tx_buf, 0xFF, sizeof(data->spi_tx_buf));
	(void)sd_spi_transceive(dev, data->spi_tx_buf, NULL, 10U);
	(void)spi_release_dt(&config->bus);
}

static void sd_command_locked(const struct device *dev, uint8_t cmd,
			      uint32_t arg, uint8_t crc)
{
	uint8_t cmd_buf[7] = {0};

	cmd_buf[0] = 0xFF;
	cmd_buf[1] = cmd | 0x40U;
	cmd_buf[2] = (uint8_t)(arg >> 24);
	cmd_buf[3] = (uint8_t)(arg >> 16);
	cmd_buf[4] = (uint8_t)(arg >> 8);
	cmd_buf[5] = (uint8_t)arg;
	cmd_buf[6] = crc | 0x01U;

	(void)sd_spi_transceive(dev, cmd_buf, NULL, sizeof(cmd_buf));
}

static uint8_t sd_read_res1_locked(const struct device *dev)
{
	struct sd_drv_data *data = dev->data;
	uint8_t i = 0U;
	uint8_t res1 = 0xFFU;

	do {
		if (sd_spi_transceive(dev, data->spi_tx_buf, &res1, 1U) != 0) {
			return 0xFFU;
		}

		i++;
		if (i > 8U) {
			break;
		}
	} while (res1 == 0xFFU);

	return res1;
}

static uint8_t sd_go_idle_state_locked(const struct device *dev)
{
	uint8_t res1;

	sd_command_locked(dev, CMD0, CMD0_ARG, CMD0_CRC);
	res1 = sd_read_res1_locked(dev);
	sd_release_bus(dev);

	return res1;
}

static void sd_read_res3_7_locked(const struct device *dev, uint8_t *res)
{
	struct sd_drv_data *data = dev->data;

	res[0] = sd_read_res1_locked(dev);
	if (res[0] > 1U) {
		return;
	}

	(void)sd_spi_transceive(dev, data->spi_tx_buf, &res[1], 4U);
}

static void sd_send_if_cond_locked(const struct device *dev, uint8_t *res)
{
	sd_command_locked(dev, CMD8, CMD8_ARG, CMD8_CRC);
	sd_read_res3_7_locked(dev, res);
	sd_release_bus(dev);
}

static void sd_read_ocr_locked(const struct device *dev, uint8_t *res)
{
	sd_command_locked(dev, CMD58, CMD58_ARG, CMD58_CRC);
	sd_read_res3_7_locked(dev, res);
	sd_release_bus(dev);
}

static uint8_t sd_send_app_locked(const struct device *dev)
{
	uint8_t res1;

	sd_command_locked(dev, CMD55, CMD55_ARG, CMD55_CRC);
	res1 = sd_read_res1_locked(dev);
	sd_release_bus(dev);

	return res1;
}

static uint8_t sd_send_op_cond_locked(const struct device *dev)
{
	uint8_t res1;

	sd_command_locked(dev, ACMD41, ACMD41_ARG, ACMD41_CRC);
	res1 = sd_read_res1_locked(dev);
	sd_release_bus(dev);

	return res1;
}

static void sd_print_data_err_token(uint8_t token)
{
	if (SD_TOKEN_OOR(token)) {
		LOG_ERR("Data out of range");
	}
	if (SD_TOKEN_CECC(token)) {
		LOG_ERR("Card ECC failed");
	}
	if (SD_TOKEN_CC(token)) {
		LOG_ERR("CC error");
	}
	if (SD_TOKEN_ERROR(token)) {
		LOG_ERR("Generic data token error");
	}
}

static void sd_log_r1(uint8_t res)
{
	if (res & 0b10000000U) {
		LOG_ERR("R1 response MSB set");
		return;
	}
	if (res == 0U) {
		LOG_DBG("Card ready");
		return;
	}
	if (PARAM_ERROR(res)) {
		LOG_ERR("Parameter error");
	}
	if (ADDR_ERROR(res)) {
		LOG_ERR("Address error");
	}
	if (ERASE_SEQ_ERROR(res)) {
		LOG_ERR("Erase sequence error");
	}
	if (CRC_ERROR(res)) {
		LOG_ERR("CRC error");
	}
	if (ILLEGAL_CMD(res)) {
		LOG_ERR("Illegal command");
	}
	if (ERASE_RESET(res)) {
		LOG_ERR("Erase reset error");
	}
	if (IN_IDLE(res)) {
		LOG_DBG("Card idle");
	}
}

static uint8_t sd_read_start_locked(const struct device *dev, uint8_t *buf,
				    uint16_t read_len, uint8_t *token)
{
	struct sd_drv_data *data = dev->data;
	uint8_t res1;
	uint8_t read = 0xFFU;
	uint16_t read_attempts = 0U;

	res1 = sd_read_res1_locked(dev);

	if (res1 == SD_READY) {
		do {
			if (sd_spi_transceive(dev, data->spi_tx_buf, &read, 1U) != 0) {
				break;
			}
			if (read_attempts == SD_MAX_READ_ATTEMPTS) {
				break;
			}
			read_attempts++;
		} while (read != SD_START_TOKEN);

		if (read == SD_START_TOKEN) {
			(void)sd_spi_transceive(dev, data->spi_tx_buf, buf, read_len);
			(void)sd_spi_transceive(dev, data->spi_tx_buf, NULL, 2U);
		}

		*token = read;
	}

	return res1;
}

static uint8_t sd_read_single_block_locked(const struct device *dev, uint32_t addr,
					   uint8_t *buf, uint8_t *token)
{
	uint8_t res1;

	*token = 0xFFU;
	sd_command_locked(dev, CMD17, addr, CMD17_CRC);
	res1 = sd_read_start_locked(dev, buf, SD_BLOCK_LEN, token);
	sd_release_bus(dev);

	return res1;
}

static uint8_t sd_write_single_block_locked(const struct device *dev,
					    uint32_t addr,
					    const uint8_t *buf,
					    uint8_t *token)
{
	struct sd_drv_data *data = dev->data;
	uint8_t write_attempts;
	uint8_t read = 0xFFU;
	uint8_t res1;
	uint8_t start_token = SD_START_TOKEN;

	*token = 0xFFU;
	sd_command_locked(dev, CMD24, addr, CMD24_CRC);
	res1 = sd_read_res1_locked(dev);

	if (res1 == SD_READY) {
		(void)sd_spi_transceive(dev, &start_token, NULL, 1U);
		(void)sd_spi_transceive(dev, buf, NULL, SD_BLOCK_LEN);

		write_attempts = 0U;
		while (write_attempts != SD_MAX_WRITE_ATTEMPTS) {
			if (sd_spi_transceive(dev, data->spi_tx_buf, &read, 1U) != 0) {
				break;
			}
			if (read != 0xFFU) {
				break;
			}
			write_attempts++;
		}

		if ((read & 0x1FU) == 0x05U) {
			*token = 0x05U;

			write_attempts = 0U;
			read = 0U;
			do {
				if (sd_spi_transceive(dev, data->spi_tx_buf, &read, 1U) != 0) {
					break;
				}
				if (write_attempts == SD_MAX_WRITE_ATTEMPTS) {
					*token = 0x00U;
					break;
				}
				write_attempts++;
			} while (read == 0U);
		}
	}

	sd_release_bus(dev);

	return res1;
}

static sd_ret_t sd_drv_card_init_locked(const struct device *dev)
{
	const struct sd_drv_config *config = dev->config;
	struct sd_drv_data *data = dev->data;
	uint8_t res[5] = {0};
	uint8_t cmd_attempts = 0U;

	if (data->initialized) {
		return SD_INIT_SUCCESS;
	}

	if (!spi_is_ready_dt(&config->bus)) {
		return SD_INIT_ERROR;
	}

	sd_power_up_seq_locked(dev);

	while ((res[0] = sd_go_idle_state_locked(dev)) != 0x01U) {
		cmd_attempts++;
		if (cmd_attempts > 50U) {
			if (res[0] == 0U) {
				LOG_ERR("Card not found");
			} else {
				sd_log_r1(res[0]);
			}
			return SD_INIT_ERROR;
		}
	}

	sd_send_if_cond_locked(dev, res);
	if (res[0] != 0x01U) {
		sd_log_r1(res[0]);
		return SD_INIT_ERROR;
	}

	if (res[4] != 0xAAU) {
		LOG_ERR("Unexpected CMD8 echo pattern: 0x%02x", res[4]);
		return SD_INIT_ERROR;
	}

	cmd_attempts = 0U;
	do {
		if (cmd_attempts > 100U) {
			return SD_INIT_ERROR;
		}

		res[0] = sd_send_app_locked(dev);
		if (res[0] < 2U) {
			res[0] = sd_send_op_cond_locked(dev);
		}

		k_sleep(K_MSEC(10));
		cmd_attempts++;
	} while (res[0] != SD_READY);

	sd_read_ocr_locked(dev, res);
	if (!POWER_UP_STATUS(res[1])) {
		LOG_ERR("Card did not power up");
		return SD_INIT_ERROR;
	}

	if (CCS_VAL(res[1])) {
		LOG_INF("Card Type: SDHC");
	} else {
		LOG_WRN("Card does not report SDHC CCS");
	}

	data->initialized = true;
	return SD_INIT_SUCCESS;
}

static sd_ret_t sd_drv_card_init_impl(const struct device *dev)
{
	struct sd_drv_data *data = dev->data;
	sd_ret_t ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = sd_drv_card_init_locked(dev);
	k_mutex_unlock(&data->lock);

	return ret;
}

static sd_ret_t sd_drv_read_sector_impl(const struct device *dev, uint32_t addr,
					uint8_t *buf)
{
	struct sd_drv_data *data = dev->data;
	uint8_t res1;
	uint8_t token;
	sd_ret_t ret = SD_READ_ERROR;

	if (buf == NULL) {
		return SD_READ_ERROR;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	if (sd_drv_card_init_locked(dev) != SD_INIT_SUCCESS) {
		ret = SD_INIT_ERROR;
		goto out;
	}

	res1 = sd_read_single_block_locked(dev, addr, buf, &token);
	if (res1 == SD_READY) {
		if (!(token & 0xF0U)) {
			sd_print_data_err_token(token);
		} else if (token == 0xFFU) {
			LOG_ERR("Read timeout");
		} else {
			ret = SD_READ_SUCCESS;
		}
	} else {
		sd_log_r1(res1);
	}

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

static sd_ret_t sd_drv_write_sector_impl(const struct device *dev, uint32_t addr,
					 const uint8_t *buf)
{
	struct sd_drv_data *data = dev->data;
	uint8_t token;
	uint8_t res1;
	sd_ret_t ret = SD_WRITE_ERROR;

	if (buf == NULL) {
		return SD_WRITE_ERROR;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	if (sd_drv_card_init_locked(dev) != SD_INIT_SUCCESS) {
		ret = SD_INIT_ERROR;
		goto out;
	}

	res1 = sd_write_single_block_locked(dev, addr, buf, &token);
	if (res1 == SD_READY) {
		if (token == 0x05U) {
			ret = SD_WRITE_SUCCESS;
		}
	} else {
		sd_log_r1(res1);
	}

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

static uint8_t sd_drv_read_multiple_start_impl(const struct device *dev,
					       uint32_t start_addr)
{
	struct sd_drv_data *data = dev->data;
	uint8_t res1 = 0xFFU;

	k_mutex_lock(&data->lock, K_FOREVER);

	if (sd_drv_card_init_locked(dev) != SD_INIT_SUCCESS) {
		k_mutex_unlock(&data->lock);
		return SD_INIT_ERROR;
	}

	if (data->multi_read_active) {
		k_mutex_unlock(&data->lock);
		return SD_READ_ERROR;
	}

	sd_command_locked(dev, CMD18, start_addr, CMD18_CRC);
	res1 = sd_read_res1_locked(dev);
	if (res1 == SD_READY) {
		data->multi_read_active = true;
		return res1;
	}

	sd_release_bus(dev);
	k_mutex_unlock(&data->lock);
	return res1;
}

static sd_ret_t sd_drv_read_multiple_impl(const struct device *dev, uint8_t *buf)
{
	struct sd_drv_data *data = dev->data;
	uint8_t read = 0xFFU;
	uint32_t read_attempts = 0U;

	if ((buf == NULL) || !data->multi_read_active) {
		return SD_READ_ERROR;
	}

	do {
		if (sd_spi_transceive(dev, data->spi_tx_buf, &read, 1U) != 0) {
			return SD_READ_ERROR;
		}
		if (read_attempts == SD_MAX_READ_ATTEMPTS) {
			break;
		}
		read_attempts++;
	} while (read != SD_START_TOKEN);

	if (read == SD_START_TOKEN) {
		(void)sd_spi_transceive(dev, data->spi_tx_buf, buf, SD_BLOCK_LEN);
		(void)sd_spi_transceive(dev, data->spi_tx_buf, NULL, 3U);
	}

	if (!(read & 0xF0U)) {
		sd_print_data_err_token(read);
		return SD_READ_ERROR;
	}
	if (read == 0xFFU) {
		LOG_ERR("Read timeout");
		return SD_READ_ERROR;
	}

	return SD_READ_SUCCESS;
}

static void sd_drv_read_multiple_stop_impl(const struct device *dev)
{
	struct sd_drv_data *data = dev->data;
	uint8_t read = 0U;

	if (!data->multi_read_active) {
		return;
	}

	sd_command_locked(dev, CMD12, CMD12_ARG, CMD12_CRC);
	do {
		if (sd_spi_transceive(dev, data->spi_tx_buf, &read, 1U) != 0) {
			break;
		}
	} while (read == 0U);

	sd_release_bus(dev);
	data->multi_read_active = false;
	k_mutex_unlock(&data->lock);
}

static int sd_drv_device_init(const struct device *dev)
{
	const struct sd_drv_config *config = dev->config;
	struct sd_drv_data *data = dev->data;

	if (!spi_is_ready_dt(&config->bus)) {
		LOG_ERR("SPI bus is not ready");
		return -ENODEV;
	}

	k_mutex_init(&data->lock);
	memset(data->spi_tx_buf, 0xFF, sizeof(data->spi_tx_buf));
	data->initialized = false;
	data->multi_read_active = false;

	return 0;
}

static const sd_drv_api_t api = {
	.card_init = sd_drv_card_init_impl,
	.read_sector = sd_drv_read_sector_impl,
	.write_sector = sd_drv_write_sector_impl,
	.read_multiple_start = sd_drv_read_multiple_start_impl,
	.read_multiple = sd_drv_read_multiple_impl,
	.read_multiple_stop = sd_drv_read_multiple_stop_impl,
};

#define SD_DRV_DEFINE(inst)                                                     \
	static struct sd_drv_data sd_drv_data_##inst;                          \
	static const struct sd_drv_config sd_drv_config_##inst = {             \
		.bus = SPI_DT_SPEC_INST_GET(                                   \
			inst, SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB |          \
				      SPI_WORD_SET(8) | SPI_HOLD_ON_CS |     \
				      SPI_LOCK_ON),                         \
	};                                                                  \
	DEVICE_DT_INST_DEFINE(inst, sd_drv_device_init, NULL,                 \
			      &sd_drv_data_##inst, &sd_drv_config_##inst,      \
			      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, \
			      &api);

DT_INST_FOREACH_STATUS_OKAY(SD_DRV_DEFINE)
