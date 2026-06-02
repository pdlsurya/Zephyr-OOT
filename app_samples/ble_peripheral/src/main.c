/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/drivers/gpio.h>
#include <sh1106_gfx.h>
const struct device *sh1106 = DEVICE_DT_GET(DT_NODELABEL(sh1106));


//static const struct gpio_dt_spec adv_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
//static const struct gpio_dt_spec conn_led = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

/* Custom Service Variables */
#define BT_UUID_CUSTOM_SERVICE_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)

static struct bt_uuid_128 my_uuid = BT_UUID_INIT_128(
	BT_UUID_CUSTOM_SERVICE_VAL);

static struct bt_uuid_128 chrc1_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2));

static struct bt_uuid_128 chrc2_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1));

static uint8_t chrc1_val[23] = {0};
static uint8_t chrc2_val[23] = {0};
static uint8_t notif_cnt;

void set_conn_led()
{
	//gpio_pin_set_dt(&conn_led, 1);
	//gpio_pin_set_dt(&adv_led, 0);
}

void set_adv_led()
{
	//gpio_pin_set_dt(&adv_led, 1);
	//gpio_pin_set_dt(&conn_led, 0);
}

static void my_ccc_cfg_changed(const struct bt_gatt_attr *attr,
							   uint16_t value)
{
	ARG_UNUSED(attr);

	bool notif_enabled = (value == BT_GATT_CCC_NOTIFY);

	printk("Indications %s\n", notif_enabled ? "enabled" : "disabled");
}

static ssize_t on_read_chrc1(struct bt_conn *conn,
							 const struct bt_gatt_attr *attr, void *buf,
							 uint16_t len, uint16_t offset)
{
	printk("Value read!\n");

	return bt_gatt_attr_read(conn, attr, buf, len, offset, chrc1_val,
							 strlen(chrc1_val));
}

static inline void oled_disp(const char *msg)
{

	oled_clearDisplay(sh1106);
	oled_printString(sh1106, msg, 0, 0, 6, 0);
	oled_display(sh1106);
}

static ssize_t on_write_chrc2(struct bt_conn *conn, const struct bt_gatt_attr *attr,
							  const void *buf, uint16_t len, uint16_t offset,
							  uint8_t flags)
{
	char *value = attr->user_data;
	memcpy(value + offset, buf, len);
	value[offset + len] = 0;
	printk("received=%s\n", value);
	oled_printLog(sh1106, value);
	oled_display(sh1106);

	return len;
}

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_CUSTOM_SERVICE_VAL)};

BT_GATT_SERVICE_DEFINE(my_svc,
					   BT_GATT_PRIMARY_SERVICE(&my_uuid),
					   BT_GATT_CHARACTERISTIC(&chrc2_uuid.uuid,
											  BT_GATT_CHRC_WRITE,
											  BT_GATT_PERM_WRITE,
											  NULL, on_write_chrc2, chrc2_val),
					   BT_GATT_CHARACTERISTIC(&chrc1_uuid.uuid,
											  (BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY),
											  BT_GATT_PERM_READ,
											  on_read_chrc1, NULL, chrc1_val),
					   BT_GATT_CCC(my_ccc_cfg_changed,
								   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err)
	{
		printk("Connection failed (err 0x%02x)\n", err);
		oled_disp("Connection failed!");
	}
	else
	{
		set_conn_led();
		printk("Connected\n");
		oled_disp("BLE Connected :)");
		oled_clearDisplay(sh1106);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);
	set_adv_led();
	oled_disp("BLE Disconnected :(");
	oled_clearDisplay(sh1106);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(void)
{
	int err;

	printk("Bluetooth initialized\n");

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err)
	{
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
	set_adv_led();
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
};


int chrc1_notify(uint8_t level)
{
	int rc;
	sprintf(chrc1_val, "Notification-ESP32=%d", level);
	rc=bt_gatt_notify(NULL,&my_svc.attrs[4],chrc1_val,strlen(chrc1_val));
	return rc == -ENOTCONN ? 0 : rc;
}

int main(void)
{
	int err;
   /*
	if (!gpio_is_ready_dt(&adv_led))
		return 0;
	err = gpio_pin_configure_dt(&adv_led, GPIO_OUTPUT_INACTIVE);
	if (err < 0)
		return 0;
		*/
    /*
	if (!gpio_is_ready_dt(&conn_led))
		return 0;
	err = gpio_pin_configure_dt(&conn_led, GPIO_OUTPUT_INACTIVE);
	if (err < 0)
		return 0;
    */
	if (!device_is_ready(sh1106))
	{
		printk("Failed to initialize sh1106 display\n");
		while (1)
			;
	}
	oled_displayOn(sh1106);
	oled_disp("BLE Advertising!");

	err = bt_enable(NULL);
	if (err)
	{
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	bt_ready();

	bt_conn_auth_cb_register(&auth_cb_display);

	/* Implement notification. At the moment there is no suitable way
	 * of starting delayed work so we do it here
	 */
	while (1)
	{
		chrc1_notify(notif_cnt++);
		k_sleep(K_MSEC(1500));
	}
	return 0;
}
