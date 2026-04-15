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

#ifndef __INA260_H
#define __INA260_H

#include "stm32h5xx_hal.h"

#define INA260_DEV_ADDR     0x44

/* INA260 寄存器地址 */
#define INA260_REG_CONFIG          0x00
#define INA260_REG_CURRENT         0x01
#define INA260_REG_BUSVOLTAGE      0x02
#define INA260_REG_POWER           0x03
#define INA260_REG_MASK_ENABLE     0x06
#define INA260_REG_ALERT_LIMIT     0x07
#define INA260_REG_MANUFACTURER_ID 0xFE
#define INA260_REG_DIE_ID          0xFF

/* 默认配置值 (连续测量电流+总线电压, 转换时间1.1ms, 平均1次) */
#define INA260_CONFIG_DEFAULT      0x6887    //6905  6B47

/* 固定LSB值 (直接由硬件决定) */
#define INA260_CURRENT_LSB         0.00125f   /* 1.25 mA */
#define INA260_BUSVOLTAGE_LSB      0.00125f   /* 1.25 mV */
#define INA260_POWER_LSB           0.010f     /* 10 mW */

/* 数据结构体 */
typedef struct {
    float current_a;        /* 电流 (A) */
    float bus_v;            /* 总线电压 (V) */
    float power_w;          /* 功率 (W) */
    uint16_t mfr_id;        /* 厂商ID (应为0x5449) */
    uint16_t die_id;        /* 设备ID (应为0x2270) */
    HAL_StatusTypeDef status;
} INA260_DataTypeDef;

typedef enum {
    INA260_DATA_CURRENT = 0,
    INA260_DATA_BUS_VOLTAGE,
    INA260_DATA_POWER,
    INA260_DATA_MFR_ID,
    INA260_DATA_DIE_ID,
} INA260_Data_Type;

/* 公共函数声明 */
HAL_StatusTypeDef INA260_Init(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr);
HAL_StatusTypeDef INA260_ReadData(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                 float *value,INA260_Data_Type data_type);
HAL_StatusTypeDef INA260_ReadAllData(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                     INA260_DataTypeDef *data);
HAL_StatusTypeDef INA260_ReadManufacturerID(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                            uint16_t *mfr_id);
HAL_StatusTypeDef INA260_ReadDieID(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                   uint16_t *die_id);
HAL_StatusTypeDef INA260_ReadRawData(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                 uint16_t *reg16, INA260_Data_Type data_type);

#endif