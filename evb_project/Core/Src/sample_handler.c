

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
#include "INA238.h"
#include "INA260.h"
#include "adc_def.h"
#include "dac_driver.h"
#include "proto_pkg.h"
#include "usbd_customhid_if.h"

extern SMBUS_HandleTypeDef hsmbus1;
extern SMBUS_HandleTypeDef hsmbus2;
extern SPI_HandleTypeDef hspi1;
extern USBD_HandleTypeDef hUsbDeviceFS;

SampleDataPacket_t sample_pkg;
DAC_HandleTypeDef hdac_dac[3]; // SS0 (PA4)
/* 设备地址列表 */
const uint8_t i2c1_dev_addrs[I2C1_INA238_NUM] = {0x40, 0x41, 0x42, 0x43, 0x44,
						 0x45, 0x46, 0x47, 0x48, 0x49,
						 0x4A, 0x4B, 0x4C
						};
const uint8_t i2c2_dev_addrs[I2C2_INA238_NUM] = {0x40, 0x41, 0x42, 0x43, 0x45,
						 0x46, 0x47, 0x48, 0x49, 0x4A,
						 0x4B, 0x4C, 0x4D
						};

void InitSampleDev()
{
	// 初始化三个DAC设备
	DAC_Init(&hdac_dac[0], DAC_TYPE_7568, GPIOA, GPIO_PIN_4, &hspi1);
	DAC_Init(&hdac_dac[1], DAC_TYPE_7568, GPIOA, GPIO_PIN_3, &hspi1);
	DAC_Init(&hdac_dac[2], DAC_TYPE_7563, GPIOA, GPIO_PIN_2, &hspi1);

	/* 显式禁用内部参考，确保使用外部参考 */
	DAC_DisableInternalRef(&hdac_dac[0]);
	DAC_DisableInternalRef(&hdac_dac[1]);
	DAC_DisableInternalRef(&hdac_dac[2]);

	// INA260初始化
	INA260_Init(&hsmbus2, INA260_DEV_ADDR);

	// INA238初始化
	for (uint8_t i = 0; i < I2C1_INA238_NUM; i++) {
		//每个设备独立初始化
		INA238_Init(&hsmbus1, i2c1_dev_addrs[i], R_SHUNT1, MAX_CURRENT1);
	}

	for (uint8_t i = 0; i < I2C2_INA238_NUM; i++) {
		//每个设备独立初始化
		INA238_Init(&hsmbus2, i2c2_dev_addrs[i], R_SHUNT2, MAX_CURRENT2);
	}

	//等待配置生效
	HAL_Delay(2);


	for (uint8_t i = 0; i < 8; i++) {
		//每个设备独立初始化
		DAC_SetVoltage(&hdac_dac[0], i, 0);
		DAC_SetVoltage(&hdac_dac[1], i, 0);
	}

	DAC_SetVoltage(&hdac_dac[2], 0, 0);
	DAC_SetVoltage(&hdac_dac[2], 1, 0);
	//开启中断定时器
	//HAL_TIM_Base_Start_IT(&htim1);

}

void ClearSampleData()
{
	memset(&sample_pkg, 0, sizeof(SampleDataPacket_t));
}

void SendSampleReport()
{
	sample_pkg.report_id = ADC_REPORT_ID;
	sample_pkg.timestamp = HAL_GetTick();
	USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, (uint8_t *)&sample_pkg, SAMPLE_REPORT_SIZE);
}


void SampleCurrentData(){
  // 读取 I2C1
  for (uint8_t i = 0; i < I2C1_INA238_NUM; i++) {
    INA238_ReadRawData(&hsmbus1, i2c1_dev_addrs[i], &sample_pkg.channel_curr_reg[i],
                    INA238_DATA_CURRENT,MAX_CURRENT1);
  }
  // 读取 I2C2
  for (uint8_t i = 0; i < I2C2_INA238_NUM; i++) {
    INA238_ReadRawData(&hsmbus2, i2c2_dev_addrs[i], &sample_pkg.channel_curr_reg[I2C1_INA238_NUM+i],
                    INA238_DATA_CURRENT,MAX_CURRENT2);
  }
  INA260_ReadRawData(&hsmbus2, INA260_DEV_ADDR, &sample_pkg.channel_curr_reg[I2C1_INA238_NUM+I2C2_INA238_NUM],
                  INA260_DATA_CURRENT);
    
}

void SampleVoltageData(){
  // 读取 I2C1
  for (uint8_t i = 0; i < I2C1_INA238_NUM; i++) {
    INA238_ReadRawData(&hsmbus1, i2c1_dev_addrs[i], &sample_pkg.channel_volt_reg[i],
                    INA238_DATA_VBUS,MAX_CURRENT1);
  }
  // 读取 I2C2
  for (uint8_t i = 0; i < I2C2_INA238_NUM; i++) {
    INA238_ReadRawData(&hsmbus2, i2c2_dev_addrs[i], &sample_pkg.channel_volt_reg[I2C1_INA238_NUM+i],
                    INA238_DATA_VBUS,MAX_CURRENT2);
  }
  INA260_ReadRawData(&hsmbus2, INA260_DEV_ADDR, &sample_pkg.channel_volt_reg[I2C1_INA238_NUM+I2C2_INA238_NUM],
                  INA260_DATA_BUS_VOLTAGE);
  
}
