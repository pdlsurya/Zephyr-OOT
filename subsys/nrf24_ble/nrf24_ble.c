/*
 * Copyright (c) 2026 Surya Poudel
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr_oot/subsys/nrf24_ble.h>

static uint8_t refresh;
static uint8_t device_index;
static bool match;
static uint8_t device_id[4];

static nrf24_ble_instance_t ble_instance;
static uint8_t *pdu_service_data_ptr;
static const uint8_t channel[3] = {37, 38, 39};
static const uint8_t frequency[3] = {2, 26, 80};
static const uint8_t access_address[4] = {0x71, 0x91, 0x7D, 0x6B};
static const uint8_t mac_address[6] = {0xfd, 0x5c, 0x1a, 0xa8, 0xef, 0xca};

static uint32_t get_device_sum(uint8_t *device_name)
{
	return (uint32_t)(device_name[6]);
}

static void reverse_bit_order(uint8_t *data, uint8_t length)
{
	while (length--) {
		uint8_t res = 0;

		if (*data & 0x80) {
			res |= 0x01;
		}
		if (*data & 0x40) {
			res |= 0x02;
		}
		if (*data & 0x20) {
			res |= 0x04;
		}
		if (*data & 0x10) {
			res |= 0x08;
		}
		if (*data & 0x08) {
			res |= 0x10;
		}
		if (*data & 0x04) {
			res |= 0x20;
		}
		if (*data & 0x02) {
			res |= 0x40;
		}
		if (*data & 0x01) {
			res |= 0x80;
		}

		*(data++) = res;
	}
}

void ble_set_mode(const struct device *dev, ble_mode_t mode)
{
	(void)nrf24_flush_tx(dev);
	(void)nrf24_flush_rx(dev);

	ble_instance.ble_mode = mode;

	switch (mode) {
	case BLE_MODE_ADVERTISE:
		(void)nrf24_set_mode(dev, TX_MODE);
		break;
	case BLE_MODE_LISTEN:
		(void)nrf24_set_mode(dev, RX_MODE);
		break;
	default:
		break;
	}
}

void ble_hop_channel(const struct device *dev)
{
	ble_instance.current_freq_index++;
	if (ble_instance.current_freq_index >= ARRAY_SIZE(channel)) {
		ble_instance.current_freq_index = 0;
	}

	(void)nrf24_set_channel(dev, frequency[ble_instance.current_freq_index]);
}

static void add_adv_data(uint8_t data_size, uint8_t type, uint8_t *data)
{
	adv_data_t *ptr = (adv_data_t *)(ble_instance.adv_pdu_buffer.payload +
					 ble_instance.adv_pl_size);

	ptr->length = data_size + 1;
	ptr->type = type;

	for (uint8_t i = 0; i < data_size; i++) {
		ptr->data[i] = data[i];
	}

	ble_instance.adv_pl_size += data_size + 2;
}

static void prepare_adv_pdu(void)
{
	ble_instance.adv_pl_size = 0;
	memset(&ble_instance.adv_pdu_buffer, 0, sizeof(ble_instance.adv_pdu_buffer));

	add_adv_data(1, 0x01, &ble_instance.adv_config_data->flags);
	add_adv_data(strlen(ble_instance.adv_config_data->adv_name), 0x09,
		     (uint8_t *)ble_instance.adv_config_data->adv_name);

	pdu_service_data_ptr =
		ble_instance.adv_pdu_buffer.payload + ble_instance.adv_pl_size + 4;
	add_adv_data(SERVICE_DATA_SIZE, 0x16,
		     (uint8_t *)ble_instance.adv_config_data->ble_service_data);

	memcpy(ble_instance.adv_pdu_buffer.mac_address, mac_address,
	       sizeof(mac_address));

	ble_instance.adv_pl_size += sizeof(mac_address);
	ble_instance.adv_pdu_buffer.header = 0x22;
	ble_instance.adv_pdu_buffer.payload_length = ble_instance.adv_pl_size;
}

static void whiten_data(uint8_t *data, uint8_t length)
{
	uint8_t lfsr = channel[ble_instance.current_freq_index];

	lfsr |= 0x40;

	while (length--) {
		uint8_t res = 0;

		for (uint8_t i = 0; i < 8; i++) {
			res |= (lfsr & 0x01) << i;

			if (lfsr & 0x01) {
				lfsr >>= 1;
				lfsr ^= 0x44;
			} else {
				lfsr >>= 1;
			}
		}

		*(data++) ^= res;
	}
}

static void compute_crc(uint8_t *data, uint8_t *crc_buf, uint8_t len)
{
	uint32_t crc = 0xAAAAAA;

	while (len--) {
		uint8_t byte = *(data++);

		for (uint8_t i = 0; i < 8; i++) {
			if ((byte & 0x01) ^ (crc & 0x01)) {
				crc >>= 1;
				crc ^= 0xDA6000;
			} else {
				crc >>= 1;
			}

			byte >>= 1;
		}
	}

	for (uint8_t i = 0; i < 3; i++) {
		crc_buf[i] = ((uint8_t *)&crc)[i];
	}
}

static void update_adv_pdu(void)
{
	*pdu_service_data_ptr = ble_instance.adv_config_data->ble_service_data->data;

	compute_crc((uint8_t *)&ble_instance.adv_pdu_buffer,
		    ble_instance.adv_pdu_buffer.payload +
			    ble_instance.adv_pl_size - sizeof(mac_address),
		    ble_instance.adv_pl_size + 2);

	whiten_data((uint8_t *)&ble_instance.adv_pdu_buffer,
		    ble_instance.adv_pl_size + 5);
	reverse_bit_order((uint8_t *)&ble_instance.adv_pdu_buffer,
			  ble_instance.adv_pl_size + 5);
}

bool ble_advertise(const struct device *dev)
{
	if (ble_instance.ble_mode != BLE_MODE_ADVERTISE) {
		printk("Device not in advertising mode!\n");
		return false;
	}

	if (ble_instance.adv_config_data == NULL) {
		printk("BLE advertising is not configured\n");
		return false;
	}

	update_adv_pdu();

	if (ble_instance.adv_pl_size > 27) {
		printk("Oversized payload!\n");
		return false;
	}

	return nrf24_tx(dev, (uint8_t *)&ble_instance.adv_pdu_buffer,
			ble_instance.adv_pl_size + 5) == 0;
}

void ble_adv_listen(const struct device *dev)
{
	uint8_t name_len = 0;
	uint8_t payload_size = 0;
	uint8_t pipe;

	if (ble_instance.ble_mode != BLE_MODE_LISTEN) {
		printk("Not in RX mode\n");
		return;
	}

	if (!nrf24_available(dev, &pipe)) {
		return;
	}

	printk("BLE device available on pipe %u\n", pipe);
	memset(&ble_instance.scan_pdu_buffer, 0, sizeof(ble_instance.scan_pdu_buffer));
	if (nrf24_rx(dev, (uint8_t *)&ble_instance.scan_pdu_buffer,
		     NRF24_MAX_PAYLOAD_SIZE) != 0) {
		printk("RX failed\n");
		return;
	}

	reverse_bit_order((uint8_t *)&ble_instance.scan_pdu_buffer,
			  NRF24_MAX_PAYLOAD_SIZE);
	whiten_data((uint8_t *)&ble_instance.scan_pdu_buffer,
		    NRF24_MAX_PAYLOAD_SIZE);
	payload_size = ble_instance.scan_pdu_buffer.payload_length -
		       sizeof(ble_instance.scan_pdu_buffer.mac_address);

	if (payload_size > 21U) {
		printk("ERROR:oversized payload\n");
		return;
	}

	{
		uint8_t received_crc[3];
		uint8_t crc[3];

		for (uint8_t i = 0; i < 3; i++) {
			received_crc[i] =
				ble_instance.scan_pdu_buffer.payload[payload_size + i];
		}

		compute_crc((uint8_t *)&ble_instance.scan_pdu_buffer, crc,
			    payload_size + 8);

		for (uint8_t i = 0; i < 3; i++) {
			if (crc[i] != received_crc[i]) {
				printk("ERROR:CRC failed\n");
				return;
			}
		}
	}

	printk("CRC OK!\n");
	printk("MAC address:->");
	for (uint8_t i = 0; i < sizeof(ble_instance.scan_pdu_buffer.mac_address);
	     i++) {
		printk("%x",
		       ble_instance.scan_pdu_buffer
			       .mac_address[sizeof(ble_instance.scan_pdu_buffer.mac_address) -
					    1U - i]);
		if (i != (sizeof(ble_instance.scan_pdu_buffer.mac_address) - 1U)) {
			printk(":");
		}
	}
	printk("\n");

	for (uint8_t index = 0; index < payload_size;) {
		adv_data_t *ptr = (adv_data_t *)(ble_instance.scan_pdu_buffer.payload +
						 index);

		if (ptr->type == 0x09) {
			char dev_name[22] = {0};

			name_len = ptr->length - 1;
			if (name_len >= sizeof(dev_name)) {
				name_len = sizeof(dev_name) - 1U;
			}

			memcpy(dev_name, ptr->data, name_len);
			printk("Device name:-> %s\n", dev_name);

			/*
			 * Retain the original scan bookkeeping so the demo behavior
			 * stays familiar if you re-enable the OLED display path later.
			 */
			if (get_device_sum((uint8_t *)dev_name) == device_id[device_index]) {
				match = true;
			}

			if (!match) {
				device_id[device_index] =
					get_device_sum((uint8_t *)dev_name);
				device_index++;
				if (device_index >= ARRAY_SIZE(device_id)) {
					device_index = 0;
				}
			}

			refresh++;
			if (refresh == 15U) {
				refresh = 0;
				memset(device_id, 0, sizeof(device_id));
				device_index = 0;
			}
			match = false;

			return;
		}

		index += ptr->length + 1;
	}

	printk("Name not available\n");
}

bool ble_begin(const struct device *dev, ble_adv_config_t *adv_config_data)
{
	if ((dev == NULL) || !device_is_ready(dev)) {
		printk("nRF24 device is not ready\n");
		return false;
	}

	memset(&ble_instance, 0, sizeof(ble_instance));

	if (nrf24_set_address_width(dev, 4U) != 0 ||
	    nrf24_set_auto_ack(dev, false) != 0 ||
	    nrf24_set_arc_count(dev, 0U) != 0 ||
	    nrf24_set_retransmit_delay(dev, 0U) != 0 ||
	    nrf24_set_tx_power(dev, TX_POWER_MAX) != 0 ||
	    nrf24_set_tx_address(dev, access_address) != 0 ||
	    nrf24_set_rx_address(dev, 0U, access_address) != 0 ||
	    nrf24_disable_crc(dev) != 0 ||
	    nrf24_set_data_rate(dev, DR_1MBPS) != 0 ||
	    nrf24_set_channel(dev, frequency[ble_instance.current_freq_index]) != 0 ||
	    nrf24_set_payload_size(dev, NRF24_MAX_PAYLOAD_SIZE) != 0 ||
	    nrf24_power_up(dev) != 0) {
		printk("Failed to configure nRF24 BLE mode\n");
		return false;
	}

	ble_instance.adv_config_data = adv_config_data;
	ble_instance.ble_mode =
		(adv_config_data != NULL) ? BLE_MODE_ADVERTISE : BLE_MODE_LISTEN;

	if (adv_config_data != NULL) {
		prepare_adv_pdu();
	}

	ble_set_mode(dev, ble_instance.ble_mode);

	return true;
}
