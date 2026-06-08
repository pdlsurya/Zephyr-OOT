/**
 * @file sh1106_drv.c
 * @author Surya Poudel 
 * @brief sh1106 oled driver
 * @version 0.1
 * @date 2023-07-05
 * 
 * @copyright Copyright (c) 2023, Surya Poudel
 * 
 * SPDX-License-Identifier: Apache-2.0
 */


#define  DT_DRV_COMPAT sinowealth_sh1106_custom

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <sh1106_drv.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sh1106_driver, CONFIG_DISPLAY_LOG_LEVEL);

#define DISP_ON              0xAF
#define DISP_OFF             0xAE
#define NORM_MODE            0xA6
#define REV_MODE             0xA7
#define PAGE_ADDRESSING_MODE 0x02
#define CONTRAST_CTRL_MODE   0X81
#define TYPE_CMD             0x00
#define TYPE_DATA            0x40
#define CMD_ROL              0xA1
#define CMD_SCAN_COM63       0xC8
#define CMD_START_RMW        0xE0 // Read-Modify-Write start
#define CMD_STOP_RMW         0xEE // Read-Modify-Write end

#define FRAME_BUFFER ((uint8_t (*)[128])dev->data)

static uint8_t i2c_tx_buf_temp[129];

static uint8_t frameBuffer[1024];

static void sendCommand(const struct device *dev, uint8_t cmd)
{
	const sh1106_config_t *config = dev->config;
	uint8_t cmd_buf[2] = {0};
	cmd_buf[0] = TYPE_CMD;
	cmd_buf[1] = cmd;

	i2c_write_dt(&config->bus, cmd_buf, 2);
}

static void setCursorPos(const struct device *dev, uint8_t x, uint8_t y)
{

	// the SH1106 display starts at x = 2! (there are two columns of off screen pixels)
	x += 2;
	sendCommand(dev, 0xB0 + (y >> 3));
	// set lower column address  (00H - 0FH) => need the upper half only - THIS IS THE x, 0->127
	sendCommand(dev, (x & 0x0F));
	// set higher column address (10H - 1FH) => 0x10 | (2 >> 4) = 10
	sendCommand(dev, 0x10 + (x >> 4));
}

static void writeBuffer(const struct device *dev)
{
	const sh1106_config_t* config=dev->config;
	for (uint8_t page = 0; page < 8; page++) {
		i2c_tx_buf_temp[0] = TYPE_DATA;
		memcpy(&i2c_tx_buf_temp[1], FRAME_BUFFER[page], 128);

		setCursorPos(dev, 0, page * 8);
		i2c_write_dt(&config->bus, i2c_tx_buf_temp, 129);
	}
}

static void setPageMode(const struct device *dev)
{
	sendCommand(dev, 0x20);
	sendCommand(dev, PAGE_ADDRESSING_MODE);
}

static void setChargePump(const struct device *dev)
{
	sendCommand(dev, 0x8d);
	sendCommand(dev, 0x14);
}

static void setLineAddress(const struct device *dev, uint8_t Address)
{

	sendCommand(dev, 0x40 | Address);
}

static void displayOn(const struct device *dev)
{
	sendCommand(dev, DISP_ON);
}

static void displayOff(const struct device *dev)
{
	sendCommand(dev, DISP_OFF);
}

static void setContrast(const struct device *dev, uint8_t contrast)
{
	sendCommand(dev, CONTRAST_CTRL_MODE);
	sendCommand(dev, contrast);
}

static void updatePage(const struct device *dev, uint8_t page)
{
	const sh1106_config_t *config = dev->config;
	setCursorPos(dev,0, page * 8);
	uint8_t temp_buff[129] = {0};
	temp_buff[0] = TYPE_DATA;
	for (uint8_t col = 0; col < 128; col++) {
		temp_buff[col + 1] = FRAME_BUFFER[page][col];

	}
	i2c_write_dt(&config->bus, temp_buff, 129);
}

static int sh1106_init(const struct device *dev)
{
	const sh1106_config_t *config = dev->config;
	if (!device_is_ready(config->bus.bus)) {
		LOG_ERR("I2C device is not ready\n");
		return -1;
	}

	if (i2c_configure(config->bus.bus, config->i2c_cfg)) {
		LOG_ERR("I2C config failed\n");
		return -1;
	}

	sendCommand(dev, NORM_MODE);
	sendCommand(dev, CMD_ROL);        // Rotate 90 degrees
	sendCommand(dev, CMD_SCAN_COM63); // start scan from COM63 to COM0
	setLineAddress(dev, 0);
	setPageMode(dev);
	setChargePump(dev);

	return 0;
}

static sh1106_api api = {
	.display_on = displayOn,
	.display_off = displayOff,
	.set_contrast = setContrast,
	.write_buffer = writeBuffer,
	.update_page = updatePage,
	.set_line_address = setLineAddress,
};

static sh1106_config_t config = {.bus = {I2C_DT_SPEC_GET_ON_I2C(DT_NODELABEL(sh1106))},
				 .i2c_cfg = I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER};

#define SH1106_DEFINE(n)\
DEVICE_DT_INST_DEFINE(n, sh1106_init, NULL, frameBuffer, &config, POST_KERNEL,CONFIG_DISPLAY_INIT_PRIORITY, &api);

DT_INST_FOREACH_STATUS_OKAY(SH1106_DEFINE);

