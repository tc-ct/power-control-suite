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

#include "ina260.h"
#include <string.h>
#include "user_callback.h"
/* 传输超时 (ms) */
#define INA260_TIMEOUT          100
#define INA260_CONV_TIMEOUT     50



/* -------------------------- 私有函数 -------------------------- */
static HAL_StatusTypeDef WaitForFlag(volatile uint8_t *flag, uint32_t timeout) {
    uint32_t tick = HAL_GetTick();
    while (*flag == 0) {
        if ((HAL_GetTick() - tick) > timeout) {
            return HAL_TIMEOUT;
        }
    }
    *flag = 0;
    if (smbus_error) {
        smbus_error = 0;
        return HAL_ERROR;
    }
    return HAL_OK;
}

/**
 * @brief 写入16位寄存器 (中断模式，同步等待)
 * @param hsmbus    SMBUS句柄
 * @param dev_addr  7位设备地址
 * @param reg_addr  寄存器地址
 * @param reg_data  待写入数据 (主机小端，函数自动转为大端)
 */
static HAL_StatusTypeDef WriteReg_IT(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                     uint8_t reg_addr, uint16_t reg_data) {
    uint8_t tx_buf[3];
    tx_buf[0] = reg_addr;
    tx_buf[1] = (reg_data >> 8) & 0xFF;   /* 高字节在前 (MSB) */
    tx_buf[2] = reg_data & 0xFF;

    smbus_tx_complete = 0;
    smbus_error = 0;
    HAL_StatusTypeDef status = HAL_SMBUS_Master_Transmit_IT(hsmbus, dev_addr << 1,
                                                            tx_buf, 3,
                                                            SMBUS_FIRST_AND_LAST_FRAME_NO_PEC);
    if (status != HAL_OK) return status;
    return WaitForFlag(&smbus_tx_complete, INA260_TIMEOUT);
}

/**
 * @brief 读取16位寄存器 (中断模式，同步等待)
 * @param hsmbus    SMBUS句柄
 * @param dev_addr  7位设备地址
 * @param reg_addr  寄存器地址
 * @param reg_data  输出数据 (直接组合，不交换)
 */
static HAL_StatusTypeDef ReadReg16_IT(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                      uint8_t reg_addr, uint16_t *reg_data) {
    uint8_t rx_buf[2];
    HAL_StatusTypeDef status;

    /* 1. 发送寄存器地址，无停止位 */
    smbus_tx_complete = 0;
    status = HAL_SMBUS_Master_Transmit_IT(hsmbus, dev_addr << 1, &reg_addr, 1,
                                          SMBUS_FIRST_FRAME);
    if (status != HAL_OK) return status;
    status = WaitForFlag(&smbus_tx_complete, INA260_TIMEOUT);
    if (status != HAL_OK) return status;

    /* 2. 接收2字节数据，带停止位 */
    smbus_rx_complete = 0;
    status = HAL_SMBUS_Master_Receive_IT(hsmbus, dev_addr << 1, rx_buf, 2,
                                         SMBUS_LAST_FRAME_NO_PEC);
    if (status != HAL_OK) return status;
    status = WaitForFlag(&smbus_rx_complete, INA260_TIMEOUT);
    if (status == HAL_OK) {
        *reg_data = ((uint16_t)rx_buf[0] << 8) | rx_buf[1];   /* MSB first，直接组合 */
    }
    return status;
}

/**
 * @brief 等待ADC转换完成 (CVRF位 = 1)
 *        INA260 的转换完成标志位于 Mask/Enable 寄存器 (0x06) 的 bit3
 */
static HAL_StatusTypeDef WaitForConversion(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr) {
    uint16_t mask_enable;
    uint32_t tick = HAL_GetTick();
    do {
        if (ReadReg16_IT(hsmbus, dev_addr, INA260_REG_MASK_ENABLE, &mask_enable) != HAL_OK) {
            return HAL_ERROR;
        }
        if (mask_enable & 0x0008) {   /* CVRF 位 (bit3) */
            return HAL_OK;
        }
        if ((HAL_GetTick() - tick) > INA260_CONV_TIMEOUT) {
            return HAL_TIMEOUT;
        }
        HAL_Delay(1);
    } while (1);
}

/* -------------------------- 公共函数 -------------------------- */

/**
 * @brief 初始化INA260设备
 * @param hsmbus    SMBUS句柄
 * @param dev_addr  7位设备地址
 * @retval HAL_OK  成功
 * @note  1. 读取厂商ID验证通信 (期望0x5449)
 *        2. 写入默认配置 (连续测量电流+总线电压, 转换时间1.1ms, 平均1次)
 *           如果不希望修改配置，可跳过此步，但写入可确保器件处于已知状态
 */
HAL_StatusTypeDef INA260_Init(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr) {
    uint16_t mfr_id;
    HAL_StatusTypeDef status;

    /* 1. 读取厂商ID，验证通信 */
    status = ReadReg16_IT(hsmbus, dev_addr, INA260_REG_MANUFACTURER_ID, &mfr_id);
    if (status != HAL_OK) return status;
    if (mfr_id != 0x5449) return HAL_ERROR;   /* 期望 "TI" */

    /* 2. 写入默认配置 (可选，但推荐) */
    status = WriteReg_IT(hsmbus, dev_addr, INA260_REG_CONFIG, INA260_CONFIG_DEFAULT);
    return status;
}


/**
 * @brief 读取 INA260 指定类型的测量数据
 * @param hsmbus     SMBUS 句柄
 * @param dev_addr   7位设备地址
 * @param data_type  要读取的数据类型
 * @param value      输出值指针 (浮点数)
 * @return HAL_OK / HAL_ERROR / HAL_TIMEOUT
 */
HAL_StatusTypeDef INA260_ReadRawData(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                 uint16_t *reg16, INA260_Data_Type data_type)
{
    HAL_StatusTypeDef status;

    if (reg16 == NULL) return HAL_ERROR;

    /* 等待转换完成（确保数据是最新的）*/
    status = WaitForConversion(hsmbus, dev_addr);
    if (status != HAL_OK) {
        return status;
    }

    switch (data_type)
    {
        case INA260_DATA_CURRENT:
            status = ReadReg16_IT(hsmbus, dev_addr, INA260_REG_CURRENT, reg16);
            break;

        case INA260_DATA_BUS_VOLTAGE:
            status = ReadReg16_IT(hsmbus, dev_addr, INA260_REG_BUSVOLTAGE, reg16);
            /* 清除 bit15（该位始终为0） */
            *reg16 = *reg16 & 0x7FFF;   /* 返回原始寄存器值，去除bit15 */
            break;

        default:
            status = HAL_ERROR;
            break;
    }

    return status;
}

/**
 * @brief 读取 INA260 指定类型的测量数据
 * @param hsmbus     SMBUS 句柄
 * @param dev_addr   7位设备地址
 * @param data_type  要读取的数据类型
 * @param value      输出值指针 (浮点数)
 * @return HAL_OK / HAL_ERROR / HAL_TIMEOUT
 */
HAL_StatusTypeDef INA260_ReadData(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                 float *value,INA260_Data_Type data_type)
{
    uint16_t reg16;
    HAL_StatusTypeDef status;

    if (value == NULL) return HAL_ERROR;

    /* 等待转换完成（确保数据是最新的）*/
    status = WaitForConversion(hsmbus, dev_addr);
    if (status != HAL_OK) {
        return status;
    }

    switch (data_type)
    {
        case INA260_DATA_CURRENT:
            status = ReadReg16_IT(hsmbus, dev_addr, INA260_REG_CURRENT, &reg16);
            if (status == HAL_OK) {
                *value = (int16_t)reg16 * INA260_CURRENT_LSB;
            }
            break;

        case INA260_DATA_BUS_VOLTAGE:
            status = ReadReg16_IT(hsmbus, dev_addr, INA260_REG_BUSVOLTAGE, &reg16);
            if (status == HAL_OK) {
                /* 清除 bit15（该位始终为0） */
                *value = (reg16 & 0x7FFF) * INA260_BUSVOLTAGE_LSB;
            }
            break;

        case INA260_DATA_POWER:
            status = ReadReg16_IT(hsmbus, dev_addr, INA260_REG_POWER, &reg16);
            if (status == HAL_OK) {
                *value = reg16 * INA260_POWER_LSB;
            }
            break;

        case INA260_DATA_MFR_ID:
            status = ReadReg16_IT(hsmbus, dev_addr, INA260_REG_MANUFACTURER_ID, &reg16);
            if (status == HAL_OK) {
                *value = (float)reg16;   /* 厂商 ID，作为浮点数返回 */
            }
            break;

        case INA260_DATA_DIE_ID:
            status = ReadReg16_IT(hsmbus, dev_addr, INA260_REG_DIE_ID, &reg16);
            if (status == HAL_OK) {
                *value = (float)reg16;   /* 器件 ID */
            }
            break;

        default:
            status = HAL_ERROR;
            break;
    }

    return status;
}

/**
 * @brief 读取所有测量数据 (电流、电压、功率、ID)
 * @param hsmbus    SMBUS句柄
 * @param dev_addr  7位设备地址
 * @param data      输出数据结构体
 */
HAL_StatusTypeDef INA260_ReadAllData(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                     INA260_DataTypeDef *data) {
    uint16_t reg16;
    HAL_StatusTypeDef status;

    if (data == NULL) return HAL_ERROR;
    memset(data, 0, sizeof(INA260_DataTypeDef));

    /* 等待转换完成 (确保数据是最新的) */
    status = WaitForConversion(hsmbus, dev_addr);
    if (status != HAL_OK) {
        data->status = status;
        return status;
    }

    /* 1. 读取电流 (有符号，二进制补码) */
    status = ReadReg16_IT(hsmbus, dev_addr, INA260_REG_CURRENT, &reg16);
    if (status != HAL_OK) goto err;
    data->current_a = (int16_t)reg16 * INA260_CURRENT_LSB;

    /* 2. 读取总线电压 (无符号，bit15始终为0) */
    status = ReadReg16_IT(hsmbus, dev_addr, INA260_REG_BUSVOLTAGE, &reg16);
    if (status != HAL_OK) goto err;
    data->bus_v = (reg16 & 0x7FFF) * INA260_BUSVOLTAGE_LSB;   /* 清除bit15 */

    /* 3. 读取功率 (无符号) */
    status = ReadReg16_IT(hsmbus, dev_addr, INA260_REG_POWER, &reg16);
    if (status != HAL_OK) goto err;
    data->power_w = reg16 * INA260_POWER_LSB;

    /* 4. 读取厂商ID和设备ID (用于验证) */
    ReadReg16_IT(hsmbus, dev_addr, INA260_REG_MANUFACTURER_ID, &data->mfr_id);
    ReadReg16_IT(hsmbus, dev_addr, INA260_REG_DIE_ID, &data->die_id);

    data->status = HAL_OK;
    return HAL_OK;

err:
    data->status = status;
    return status;
}

/**
 * @brief 单独读取厂商ID
 */
HAL_StatusTypeDef INA260_ReadManufacturerID(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                            uint16_t *mfr_id) {
    return ReadReg16_IT(hsmbus, dev_addr, INA260_REG_MANUFACTURER_ID, mfr_id);
}

/**
 * @brief 单独读取设备ID
 */
HAL_StatusTypeDef INA260_ReadDieID(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                   uint16_t *die_id) {
    return ReadReg16_IT(hsmbus, dev_addr, INA260_REG_DIE_ID, die_id);
}