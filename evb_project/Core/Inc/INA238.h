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

#ifndef __INA238_H
#define __INA238_H

#include "stm32h5xx_hal.h"


/* 寄存器地址 (与数据手册一致) */
#define INA238_REG_CONFIG       0x00
#define INA238_REG_ADC_CONFIG   0x01
#define INA238_REG_SHUNT_CAL    0x02
#define INA238_REG_VSHUNT       0x04
#define INA238_REG_VBUS         0x05
#define INA238_REG_DIETEMP      0x06
#define INA238_REG_CURRENT      0x07
#define INA238_REG_POWER        0x08
#define INA238_REG_DIAG_ALRT    0x0B
#define INA238_REG_MANUFACTURER 0x3E
#define INA238_REG_DEVICE_ID    0x3F


/* INA238 数据结构体 */
typedef struct {
    float vshunt_mv;      /* 分流电压 (mV) */
    float vbus_v;         /* 总线电压 (V) */
    float current_a;      /* 电流 (A) */
    float power_w;        /* 功率 (W) */
    float temp_c;         /* 温度 (℃) */
    uint16_t mfr_id;      /* 厂商ID (应为0x5449) */
    uint16_t dev_id;      /* 设备ID (应为0x2381) */
    HAL_StatusTypeDef status;  /* 读取状态 */
} INA238_DataTypeDef;

/* 定义要读取的数据类型枚举 */
typedef enum {
    INA238_DATA_VSHUNT = 0,  /* 分流电压 (mV) */
    INA238_DATA_VBUS,        /* 总线电压 (V) */
    INA238_DATA_CURRENT,     /* 电流 (A) */
    INA238_DATA_POWER,       /* 功率 (W) */
    INA238_DATA_TEMP,        /* 温度 (℃) */
    INA238_DATA_MFR_ID,      /* 厂商ID */
    INA238_DATA_DEVICE_ID,   /* 器件ID */
} INA238_Data_Type;


/* 公共函数声明 */
HAL_StatusTypeDef INA238_Init(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                              float r_shunt, float max_current);
HAL_StatusTypeDef INA238_ReadData(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                  float *value,INA238_Data_Type data_type,float max_current);
HAL_StatusTypeDef INA238_ReadRawData(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                  uint16_t* reg16, INA238_Data_Type data_type,float max_current);
HAL_StatusTypeDef INA238_ReadAllData(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                     INA238_DataTypeDef *data, float current_lsb);
void INA238_ReadBusDevices(SMBUS_HandleTypeDef *hsmbus, const uint8_t *dev_addrs,
                           uint8_t dev_num, INA238_DataTypeDef *all_data,
                           float r_shunt, float max_current);

#endif