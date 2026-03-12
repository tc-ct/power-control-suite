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
 
#ifndef DAC_DRIVER_H
#define DAC_DRIVER_H

#include "stm32h5xx_hal.h"
#include "proto_pkg.h"

#define MAX_VOLTAGE_NUM 36

/* DAC芯片类型枚举 */
typedef enum {
    DAC_TYPE_7568,  // 8通道，32位帧
    DAC_TYPE_7563   // 双通道，24位帧
} DAC_Device_TypeDef;


HAL_StatusTypeDef DAC_VoltageConfig(VoltageConfigPacket_t *cfg);

HAL_StatusTypeDef DAC_PowerOnOff(uint8_t power);

#endif /* DAC_DRIVER_H */