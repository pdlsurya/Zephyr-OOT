/*
 * Copyright (c) 2026 Surya Poudel
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRF24_BLE_H
#define NRF24_BLE_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/device.h>

#include <zephyr_oot/drivers/nrf24_drv.h>

#define NRF_TEMPERATURE_SERVICE_UUID 0x1809
#define NRF_BATTERY_SERVICE_UUID 0x180F
#define NRF_DEVICE_INFORMATION_SERVICE_UUID 0x180A
#define NRF_HEART_RATE_SERVICE_UUID 0x180D
#define BLE_ADV_FLAG 0x05
#define SERVICE_DATA_SIZE sizeof(ble_service_data_t)

typedef struct {
	uint8_t header;
	uint8_t payload_length;
	uint8_t mac_address[6];
	uint8_t payload[24];
} adv_pdu_t;

typedef struct {
	uint8_t length;
	uint8_t type;
	uint8_t data[];
} adv_data_t;

typedef struct {
	uint16_t uuid;
	uint8_t data;
} ble_service_data_t;

typedef struct {
	uint8_t flags;
	const char *adv_name;
	ble_service_data_t *ble_service_data;
} ble_adv_config_t;

typedef enum {
	BLE_MODE_ADVERTISE,
	BLE_MODE_LISTEN
} ble_mode_t;

typedef struct {
	uint8_t current_freq_index;
	adv_pdu_t adv_pdu_buffer;
	adv_pdu_t scan_pdu_buffer;
	uint8_t adv_pl_size;
	ble_adv_config_t *adv_config_data;
	ble_mode_t ble_mode;
} nrf24_ble_instance_t;

bool ble_begin(const struct device *dev, ble_adv_config_t *ble_config);
void ble_adv_listen(const struct device *dev);
bool ble_advertise(const struct device *dev);
void ble_hop_channel(const struct device *dev);
void ble_set_mode(const struct device *dev, ble_mode_t mode);

#endif /* NRF24_BLE_H */
