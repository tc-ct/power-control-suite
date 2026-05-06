
/*
 * Copyright (C) 2025- FlyingChip Technology (Shanghai) Co., Ltd.
 * All rights reserved.
 *
 * This file contains information that is proprietary to FlyingChip.
 * The holder of this file shall treat all information contained herein as
 * confidential, use the information only for its intended purpose, illustrate
 * the copyright of FlyingChip and not duplicate, disclose, modificate or
 * disseminate any of this information in any manner unless FlyingChip
 * has otherwise provided express written permission.
 * Use of the file may require a license of intellectual property from FlyingChip.
 * This file conveys no express or implied licenses to any intellectual property
 * rights belonging to FlyingChip.
 *
 * ALL INFORMATION CONTAINED IN THIS FILE IS FURNISHED "AS IS".
 * FlyingChip DISCLAIMS ANY AND ALL TYPES OF WARRANTIES, EXPRESS, IMPLIED, OR
 * STATUTORY, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY OR FITNESS FOR
 * A PARTICULAR PURPOSE WITH RESPECT TO THE INFORMATION PROVIDE HEREUNDER.
 *
 * FlyingChip RESERVES ALL RIGHTS NOT EXPRESSLY GRANTED TO YOU HEREUNDER.
 */
#include "usb_message_handler.h"

#include "main.h"
#include "proto_pkg.h"
#include "stm32h5xx_hal.h"
#include "dac_driver.h"
#include "usbd_conf.h"
#include "usbd_customhid_if.h"
#include "sample_handler.h"
#include "user_callback.h"

#include <stdint.h>
#include <string.h>

extern volatile uint8_t current_sampling_enabled;
extern volatile uint8_t voltage_sampling_enabled;
extern volatile uint8_t sampling_enabled_once;
extern DAC_HandleTypeDef hdac_dac[3];
extern USBD_HandleTypeDef hUsbDeviceFS;
extern SMBUS_HandleTypeDef hsmbus1;
extern SMBUS_HandleTypeDef hsmbus2;
extern SPI_HandleTypeDef hspi1;

#define USB_MSG_QUEUE_DEPTH 32U
#define DEBUG_IO_TIMEOUT_MS 100U
#define DEBUG_SPI_TIMEOUT_MS 100U
#define DEBUG_RESP_SEND_TIMEOUT_MS 20U
#define DEBUG_RESP_SEND_RETRY_DELAY_MS 1U

#define DEBUG_MAGIC_0 ((uint8_t)'D')
#define DEBUG_MAGIC_1 ((uint8_t)'B')
#define DEBUG_MAGIC_2 ((uint8_t)'G')
#define DEBUG_MAGIC_3 ((uint8_t)'1')

typedef struct {
	uint8_t event_idx;
	uint8_t cmd_id;
	uint16_t payload_len;
	uint8_t payload[DOWNLOAD_PACKET_SIZE];
} UsbMessage_t;

static UsbMessage_t g_usb_queue[USB_MSG_QUEUE_DEPTH];
static volatile uint8_t g_usb_head = 0;
static volatile uint8_t g_usb_tail = 0;
static volatile uint8_t g_usb_count = 0;
static volatile uint32_t g_usb_dropped = 0;

static VoltageConfigPacket_t g_power_cfg[POWER_SUPPLY_COUNT];
static uint8_t g_power_index = 0;
static DebugResponsePacket_t g_debug_response[2];
static uint8_t g_debug_response_index = 0;

static uint8_t SendDebugResponseReport(uint8_t *report)
{
	uint8_t result;
	uint32_t start = HAL_GetTick();

	do {
		result = USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, report, USB_REPORT_SIZE);
		if (result != (uint8_t)USBD_BUSY)
			return result;

		HAL_Delay(DEBUG_RESP_SEND_RETRY_DELAY_MS);
	} while ((HAL_GetTick() - start) < DEBUG_RESP_SEND_TIMEOUT_MS);

	return result;
}

static void SendDebugResponse(const DebugRequestPacket_t *req,
			      uint8_t status,
			      uint8_t error_code,
			      uint32_t reg_addr,
			      const uint8_t *data,
			      uint8_t data_len)
{
	DebugResponsePacket_t *response;
	uint8_t result;

	g_debug_response_index ^= 1U;
	response = &g_debug_response[g_debug_response_index];

	memset(response, 0, sizeof(*response));
	response->report_id = ADC_REPORT_ID;
	response->magic[0] = DEBUG_MAGIC_0;
	response->magic[1] = DEBUG_MAGIC_1;
	response->magic[2] = DEBUG_MAGIC_2;
	response->magic[3] = DEBUG_MAGIC_3;
	response->req_id = (req != NULL) ? req->req_id : 0U;
	response->cmd_id = (req != NULL) ? req->cmd_id : 0U;
	response->status = status;
	response->error_code = error_code;
	response->reg_addr = reg_addr;
	if (data_len > DEBUG_RESP_MAX_DATA_LEN)
		data_len = DEBUG_RESP_MAX_DATA_LEN;
	response->data_len = data_len;
	if (data != NULL && data_len > 0U)
		memcpy(response->data, data, data_len);

	result = SendDebugResponseReport(response->bytes);
	if (result != (uint8_t)USBD_OK)
		USBD_ErrLog("Debug response send failed: result=%u\r\n", result);
}

static uint8_t DebugErrorFromHal(HAL_StatusTypeDef status)
{
	switch (status) {
		case HAL_BUSY:
			return DEBUG_ERR_HAL_BUSY;
		case HAL_TIMEOUT:
			return DEBUG_ERR_HAL_TIMEOUT;
		case HAL_OK:
			return DEBUG_ERR_NONE;
		case HAL_ERROR:
		default:
			return DEBUG_ERR_HAL_FAIL;
	}
}

static SMBUS_HandleTypeDef *GetSmbusHandle(uint8_t bus_id)
{
	switch (bus_id) {
		case DEBUG_BUS_I2C1:
			return &hsmbus1;
		case DEBUG_BUS_I2C2:
			return &hsmbus2;
		default:
			return NULL;
	}
}

static HAL_StatusTypeDef WaitSmbusComplete(volatile uint8_t *flag, uint32_t timeout_ms)
{
	const uint32_t start = HAL_GetTick();
	while (*flag == 0U) {
		if ((HAL_GetTick() - start) >= timeout_ms)
			return HAL_TIMEOUT;
	}
	return HAL_OK;
}

static HAL_StatusTypeDef SmbusTransmitBlocking(SMBUS_HandleTypeDef *hsmbus,
		uint8_t dev_addr_7bit,
		uint8_t *data,
		uint16_t len,
		uint32_t options)
{
	HAL_StatusTypeDef status;

	smbus_tx_complete = 0U;
	smbus_error = 0U;

	status = HAL_SMBUS_Master_Transmit_IT(hsmbus, (uint16_t)(dev_addr_7bit << 1), data, len, options);
	if (status != HAL_OK)
		return status;

	status = WaitSmbusComplete(&smbus_tx_complete, DEBUG_IO_TIMEOUT_MS);
	if (status != HAL_OK) {
		(void)HAL_SMBUS_Master_Abort_IT(hsmbus, (uint16_t)(dev_addr_7bit << 1));
		return status;
	}
	if (smbus_error != 0U)
		return HAL_ERROR;

	return HAL_OK;
}

static HAL_StatusTypeDef SmbusReceiveBlocking(SMBUS_HandleTypeDef *hsmbus,
		uint8_t dev_addr_7bit,
		uint8_t *data,
		uint16_t len,
		uint32_t options)
{
	HAL_StatusTypeDef status;

	smbus_rx_complete = 0U;
	smbus_error = 0U;

	status = HAL_SMBUS_Master_Receive_IT(hsmbus, (uint16_t)(dev_addr_7bit << 1), data, len, options);
	if (status != HAL_OK)
		return status;

	status = WaitSmbusComplete(&smbus_rx_complete, DEBUG_IO_TIMEOUT_MS);
	if (status != HAL_OK) {
		(void)HAL_SMBUS_Master_Abort_IT(hsmbus, (uint16_t)(dev_addr_7bit << 1));
		return status;
	}
	if (smbus_error != 0U)
		return HAL_ERROR;

	return HAL_OK;
}

static uint16_t EncodeRegAddr(uint32_t reg_addr, uint8_t reg_len, uint8_t *out)
{
	uint8_t i;
	for (i = 0U; i < reg_len; ++i) {
		uint8_t shift = (uint8_t)((reg_len - 1U - i) * 8U);
		out[i] = (uint8_t)((reg_addr >> shift) & 0xFFU);
	}
	return reg_len;
}

static GPIO_TypeDef *GetSpiCsPort(uint8_t cs_id)
{
	switch (cs_id) {
		case 0U:
		case 1U:
		case 2U:
			return GPIOA;
		default:
			return NULL;
	}
}

static uint16_t GetSpiCsPin(uint8_t cs_id)
{
	switch (cs_id) {
		case 0U:
			return GPIO_PIN_4;
		case 1U:
			return GPIO_PIN_3;
		case 2U:
			return GPIO_PIN_2;
		default:
			return 0U;
	}
}

static GPIO_TypeDef *GetPortFromIndex(uint8_t port_index)
{
	switch (port_index) {
		case 0:
			return GPIOB;

		case 1:
			return GPIOC;

		default:
			return NULL;
	}
}

static uint16_t GetPowerEnablePinMask(uint8_t pin_index)
{
	if (pin_index >= 16U)
		return 0;

	return (uint16_t)(1U << (pin_index & 0x0FU));
}

static void ExecutePowerOnSequence(const SequenceConfigPacket_t *sequence)
{
	typedef struct {
		uint8_t power_id;
		uint8_t order;
		uint8_t pin_port;
		uint8_t enable_pin;
		uint16_t delay;
	} PowerStep;

	PowerStep steps[POWER_SUPPLY_COUNT];
	uint8_t i;
	uint8_t j;

	for (i = 0; i < POWER_SUPPLY_COUNT; ++i) {
		steps[i].power_id = i;
		steps[i].order = sequence->sequence[i];
		steps[i].pin_port = g_power_cfg[i].pin_port;
		steps[i].enable_pin = g_power_cfg[i].enable_pin;
		steps[i].delay = sequence->interval_ms[i];
		USBD_DbgLog("Power ID %u: order=%u, port=%u, pin=%u, delay=%ums\r\n",
			    steps[i].power_id, steps[i].order, steps[i].pin_port,
			    steps[i].enable_pin, steps[i].delay);
	}

	for (i = 0; i < POWER_SUPPLY_COUNT - 1U; ++i) {
		for (j = 0; j < POWER_SUPPLY_COUNT - i - 1U; ++j) {
			if (steps[j].order > steps[j + 1U].order) {
				PowerStep temp = steps[j];
				steps[j] = steps[j + 1U];
				steps[j + 1U] = temp;
			}
		}
	}

	for (i = 0; i < POWER_SUPPLY_COUNT; ++i) {
		GPIO_TypeDef *port = GetPortFromIndex(steps[i].pin_port);
		uint16_t pin_mask = GetPowerEnablePinMask(steps[i].enable_pin);

		if (port != NULL && pin_mask != 0U)
			HAL_GPIO_WritePin(port, pin_mask, GPIO_PIN_SET);

		if (steps[i].delay != 0U)
			HAL_Delay(steps[i].delay);
	}
}

static void ExecutePowerOff(void)
{
	uint8_t power_id;
	uint8_t ch;

	for (power_id = 0; power_id < POWER_SUPPLY_COUNT; ++power_id) {
		GPIO_TypeDef *port = GetPortFromIndex(g_power_cfg[power_id].pin_port);
		uint16_t pin_mask = GetPowerEnablePinMask(g_power_cfg[power_id].enable_pin);

		if (port != NULL && pin_mask != 0U)
			HAL_GPIO_WritePin(port, pin_mask, GPIO_PIN_RESET);
	}

	for (ch = 0; ch < 8U; ++ch) {
		DAC_SetVoltage(&hdac_dac[0], ch, 0);
		DAC_SetVoltage(&hdac_dac[1], ch, 0);
	}

	DAC_SetVoltage(&hdac_dac[2], 0, 0);
	DAC_SetVoltage(&hdac_dac[2], 1, 0);
}

static void HandleSetVoltage(const uint8_t *payload)
{
	const VoltageConfigPacket_t *cfg = (const VoltageConfigPacket_t *)payload;

	if (g_power_index > POWER_SUPPLY_COUNT) {
		USBD_ErrLog("Power config index out of range\r\n");
		return;
	}

	if (cfg->device_id > 3U) {
		USBD_ErrLog("Invalid device_id: %u\r\n", cfg->device_id);
		return;
	}

	DAC_SetVoltage(&hdac_dac[cfg->device_id], cfg->channel, cfg->dac_value);
	g_power_cfg[g_power_index] = *cfg;
	g_power_index++;
	USBD_DbgLog("Set voltage: device_id=%u, channel=%u, dac_value=%u, pin_port=%u, enable_pin=%u\r\n",
		    cfg->device_id, cfg->channel, cfg->dac_value, cfg->pin_port, cfg->enable_pin);

}

static void HandleSetPin(const uint8_t *payload)
{
	const PinConfigPacket_t *pin = (const PinConfigPacket_t *)payload;
	GPIO_TypeDef *port = GetPortFromIndex(pin->port);

	if (port == NULL)
		return;

	{
		uint16_t pin_mask = (uint16_t)(1U << (pin->pin & 0x0F));
		GPIO_PinState state = (pin->level == 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
		HAL_GPIO_WritePin(port, pin_mask, state);
	}
}

static void HandlePowerOn(const uint8_t *payload)
{
	const SequenceConfigPacket_t *sequence = (const SequenceConfigPacket_t *)payload;
	ExecutePowerOnSequence(sequence);
}

static void HandlePowerOff(void)
{
	ExecutePowerOff();
}

static void HandleStartSampling(const uint8_t *payload)
{
	const SampleConfigPacket_t *sample = (const SampleConfigPacket_t *)payload;
	USBD_DbgLog("Start sampling: type=%u, state=%u\r\n", sample->type, sample->state);

	switch (sample->type) {
		case SAMPLE_TYPE_VOLTAGE:
			voltage_sampling_enabled = sample->state;
			break;

		case SAMPLE_TYPE_CURRENT:
			current_sampling_enabled = sample->state;
			break;

		default:
			break;
	}
}

static void HandleSamplingOnce(const uint8_t *payload)
{
	// const SampleConfigPacket_t *sample = (const SampleConfigPacket_t *)payload;
	USBD_DbgLog("Start sampling once \r\n");
	sampling_enabled_once = 1;
}

static void HandleI2cWrite(const uint8_t *payload)
{
	const DebugRequestPacket_t *req = (const DebugRequestPacket_t *)payload;
	SMBUS_HandleTypeDef *hsmbus = GetSmbusHandle(req->bus_id);
	uint8_t tx_buf[DEBUG_REQ_MAX_DATA_LEN + 4U];
	uint16_t tx_len;
	HAL_StatusTypeDef status;

	if (hsmbus == NULL) {
		SendDebugResponse(req, DEBUG_STATUS_INVALID_PARAM, DEBUG_ERR_INVALID_BUS, req->reg_addr, NULL, 0U);
		return;
	}
	if (req->reg_len == 0U || req->reg_len > 4U || req->data_len == 0U || req->data_len > DEBUG_REQ_MAX_DATA_LEN) {
		SendDebugResponse(req, DEBUG_STATUS_INVALID_PARAM, DEBUG_ERR_INVALID_LEN, req->reg_addr, NULL, 0U);
		return;
	}

	tx_len = EncodeRegAddr(req->reg_addr, req->reg_len, tx_buf);
	memcpy(tx_buf + tx_len, req->data, req->data_len);
	tx_len = (uint16_t)(tx_len + req->data_len);

	status = SmbusTransmitBlocking(hsmbus, req->target_id, tx_buf, tx_len, SMBUS_FIRST_AND_LAST_FRAME_NO_PEC);
	if (status != HAL_OK) {
		SendDebugResponse(req, (status == HAL_TIMEOUT) ? DEBUG_STATUS_TIMEOUT : DEBUG_STATUS_HAL_ERROR,
				  (smbus_error != 0U) ? DEBUG_ERR_SMBUS_ERROR : DebugErrorFromHal(status),
				  req->reg_addr, NULL, 0U);
		return;
	}

	SendDebugResponse(req, DEBUG_STATUS_OK, DEBUG_ERR_NONE, req->reg_addr, NULL, 0U);
}

static void HandleI2cRead(const uint8_t *payload)
{
	const DebugRequestPacket_t *req = (const DebugRequestPacket_t *)payload;
	SMBUS_HandleTypeDef *hsmbus = GetSmbusHandle(req->bus_id);
	uint8_t reg_buf[4U];
	uint8_t rx_buf[DEBUG_RESP_MAX_DATA_LEN];
	HAL_StatusTypeDef status;
	uint16_t reg_len;

	if (hsmbus == NULL) {
		SendDebugResponse(req, DEBUG_STATUS_INVALID_PARAM, DEBUG_ERR_INVALID_BUS, req->reg_addr, NULL, 0U);
		return;
	}
	if (req->reg_len == 0U || req->reg_len > 4U || req->data_len == 0U || req->data_len > DEBUG_RESP_MAX_DATA_LEN) {
		SendDebugResponse(req, DEBUG_STATUS_INVALID_PARAM, DEBUG_ERR_INVALID_LEN, req->reg_addr, NULL, 0U);
		return;
	}

	reg_len = EncodeRegAddr(req->reg_addr, req->reg_len, reg_buf);
	status = SmbusTransmitBlocking(hsmbus, req->target_id, reg_buf, reg_len, SMBUS_FIRST_FRAME);
	if (status != HAL_OK) {
		SendDebugResponse(req, (status == HAL_TIMEOUT) ? DEBUG_STATUS_TIMEOUT : DEBUG_STATUS_HAL_ERROR,
				  (smbus_error != 0U) ? DEBUG_ERR_SMBUS_ERROR : DebugErrorFromHal(status),
				  req->reg_addr, NULL, 0U);
		return;
	}

	status = SmbusReceiveBlocking(hsmbus, req->target_id, rx_buf, req->data_len, SMBUS_LAST_FRAME_NO_PEC);
	if (status != HAL_OK) {
		SendDebugResponse(req, (status == HAL_TIMEOUT) ? DEBUG_STATUS_TIMEOUT : DEBUG_STATUS_HAL_ERROR,
				  (smbus_error != 0U) ? DEBUG_ERR_SMBUS_ERROR : DebugErrorFromHal(status),
				  req->reg_addr, NULL, 0U);
		return;
	}

	SendDebugResponse(req, DEBUG_STATUS_OK, DEBUG_ERR_NONE, req->reg_addr, rx_buf, req->data_len);
}

static void HandleSpiWrite(const uint8_t *payload)
{
	const DebugRequestPacket_t *req = (const DebugRequestPacket_t *)payload;
	GPIO_TypeDef *cs_port;
	uint16_t cs_pin;
	HAL_StatusTypeDef status;

	if (req->bus_id != DEBUG_BUS_SPI1) {
		SendDebugResponse(req, DEBUG_STATUS_INVALID_PARAM, DEBUG_ERR_INVALID_BUS, req->reg_addr, NULL, 0U);
		return;
	}
	if (req->data_len == 0U || req->data_len > DEBUG_REQ_MAX_DATA_LEN) {
		SendDebugResponse(req, DEBUG_STATUS_INVALID_PARAM, DEBUG_ERR_INVALID_LEN, req->reg_addr, NULL, 0U);
		return;
	}

	cs_port = GetSpiCsPort(req->target_id);
	cs_pin = GetSpiCsPin(req->target_id);
	if (cs_port == NULL || cs_pin == 0U) {
		SendDebugResponse(req, DEBUG_STATUS_INVALID_PARAM, DEBUG_ERR_INVALID_CS, req->reg_addr, NULL, 0U);
		return;
	}

	HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
	status = HAL_SPI_Transmit(&hspi1, (uint8_t *)req->data, req->data_len, DEBUG_SPI_TIMEOUT_MS);
	HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

	if (status != HAL_OK) {
		SendDebugResponse(req, (status == HAL_TIMEOUT) ? DEBUG_STATUS_TIMEOUT : DEBUG_STATUS_HAL_ERROR,
				  DebugErrorFromHal(status), req->reg_addr, NULL, 0U);
		return;
	}

	SendDebugResponse(req, DEBUG_STATUS_OK, DEBUG_ERR_NONE, req->reg_addr, NULL, 0U);
}

static void HandleUnsupportedDebug(const uint8_t *payload)
{
	const DebugRequestPacket_t *req = (const DebugRequestPacket_t *)payload;
	SendDebugResponse(req, DEBUG_STATUS_UNSUPPORTED, DEBUG_ERR_NONE, req->reg_addr, NULL, 0U);
}

static void DispatchMessage(const UsbMessage_t *msg)
{
	switch (msg->cmd_id) {
		case CMD_SET_VOLTAGE:
			HandleSetVoltage(msg->payload);
			break;

		case CMD_SET_PIN:
			HandleSetPin(msg->payload);
			break;

		case CMD_POWER_ON:
			HandlePowerOn(msg->payload);
			break;

		case CMD_POWER_OFF:
			HandlePowerOff();
			break;

		case CMD_START_SAMPLING:
			HandleStartSampling(msg->payload);
			break;

		case CMD_SAMPLING_ONCE:
			HandleSamplingOnce(msg->payload);
			break;

		case CMD_I2C_WRITE:
			HandleI2cWrite(msg->payload);
			break;

		case CMD_I2C_READ:
			HandleI2cRead(msg->payload);
			break;

		case CMD_SPI_WRITE:
			HandleSpiWrite(msg->payload);
			break;

		case CMD_SPI_READ:
			HandleUnsupportedDebug(msg->payload);
			break;

		default:
			break;
	}

	/* Start next USB packet transfer once data processing is completed */
	USBD_CUSTOM_HID_ReceivePacket(&hUsbDeviceFS);
}

void UsbMsg_Reset(void)
{
	__disable_irq();
	g_usb_head = 0;
	g_usb_tail = 0;
	g_usb_count = 0;
	g_usb_dropped = 0;
	__enable_irq();
}

uint8_t UsbMsg_Enqueue(uint8_t event_idx, uint8_t cmd_id, const uint8_t *data, uint16_t len)
{
	UsbMessage_t *slot;

	if (data == NULL)
		return 0;

	if (len > DOWNLOAD_PACKET_SIZE)
		len = DOWNLOAD_PACKET_SIZE;

	__disable_irq();

	if (g_usb_count >= USB_MSG_QUEUE_DEPTH) {
		g_usb_dropped++;
		USBD_ErrLog("USB message queue full, dropping message (total dropped: %lu)\r\n", g_usb_dropped);
		__enable_irq();
		return 0;
	}

	slot = &g_usb_queue[g_usb_tail];
	slot->event_idx = event_idx;
	slot->cmd_id = cmd_id;
	slot->payload_len = len;
	memcpy(slot->payload, data, len);

	if (len < DOWNLOAD_PACKET_SIZE)
		memset(slot->payload + len, 0, DOWNLOAD_PACKET_SIZE - len);

	g_usb_tail = (uint8_t)((g_usb_tail + 1U) % USB_MSG_QUEUE_DEPTH);
	g_usb_count++;
	__enable_irq();

	return 1;
}

uint8_t UsbMsg_ProcessNext(void)
{
	UsbMessage_t msg;

	__disable_irq();

	if (g_usb_count == 0U) {
		__enable_irq();
		return 0;
	}

	msg = g_usb_queue[g_usb_head];
	g_usb_head = (uint8_t)((g_usb_head + 1U) % USB_MSG_QUEUE_DEPTH);
	g_usb_count--;
	__enable_irq();

	DispatchMessage(&msg);
	return 1;
}

uint32_t UsbMsg_GetDroppedCount(void)
{
	uint32_t dropped;

	__disable_irq();
	dropped = g_usb_dropped;
	__enable_irq();

	return dropped;
}
