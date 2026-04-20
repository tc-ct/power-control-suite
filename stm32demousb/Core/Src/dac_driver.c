/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32h5xx_hal.h"
#include <stdint.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "dac_driver.h"
/* USER CODE END Includes */

static VoltageConfigPacket_t g_vol_cfg_pkg[MAX_VOLTAGE_NUM];
static uint8_t voltage_index = 0;

HAL_StatusTypeDef DAC_VoltageConfig(VoltageConfigPacket_t *cfg)
{
	if (voltage_index > MAX_VOLTAGE_NUM)
		return HAL_ERROR;

	g_vol_cfg_pkg[voltage_index] = *cfg;
	voltage_index++;

	return HAL_OK;
}

static HAL_StatusTypeDef PowerOn()
{
	for (int i = 0; i < voltage_index; i++) {
		VoltageConfigPacket_t *cfg = &g_vol_cfg_pkg[voltage_index];
		UNUSED(cfg);
		// DAC_SetVoltage(&dac_devices[cfg->device_id], cfg->channel, cfg->voltage_mv);
		HAL_Delay(cfg->delay_ms);
	}

	// ldo enable

	return HAL_OK;
}

static HAL_StatusTypeDef PowerOff()
{
	for (int i = 0; i < voltage_index; i++) {
		VoltageConfigPacket_t *cfg = &g_vol_cfg_pkg[voltage_index];
		UNUSED(cfg);
		// DAC_SetVoltage(&dac_devices[cfg->device_id], cfg->channel, 0x00);
		HAL_Delay(cfg->delay_ms);
	}

	// ldo disable

	return HAL_OK;
}

// 电源上电/下电
HAL_StatusTypeDef DAC_PowerOnOff(uint8_t power)
{
	if (power == 1)
		PowerOn();

	else
		PowerOff();

	return HAL_OK;
}