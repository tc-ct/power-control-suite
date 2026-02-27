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

#include "main.h"
#include "ina238.h"
#include <stdio.h>
#include <string.h>
#include "smbus_callback.h"




/* I2C1 / I2C2 句柄 (由 CubeMX 生成，此处仅为 extern 声明) */
extern SMBUS_HandleTypeDef hsmbus1;
extern SMBUS_HandleTypeDef hsmbus2;

/* -------------------------- 私有函数声明 -------------------------- */
static HAL_StatusTypeDef INA238_WaitForFlag(volatile uint8_t *flag, uint32_t timeout);
static HAL_StatusTypeDef INA238_WriteReg_IT(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                            uint8_t reg_addr, uint16_t reg_data);
static HAL_StatusTypeDef INA238_ReadReg16_IT(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                             uint8_t reg_addr, uint16_t *reg_data);
static HAL_StatusTypeDef INA238_ReadReg24_IT(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                             uint8_t reg_addr, uint32_t *reg_data);
static HAL_StatusTypeDef INA238_WaitForConversion(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr);


/* -------------------------- 私有函数实现 -------------------------- */

/**
 * @brief 等待标志位，带超时
 */
static HAL_StatusTypeDef INA238_WaitForFlag(volatile uint8_t *flag, uint32_t timeout) {
    uint32_t tickstart = HAL_GetTick();
    while (*flag == 0) {
        if ((HAL_GetTick() - tickstart) > timeout) {
            return HAL_TIMEOUT;
        }
    }
    *flag = 0;  /* 清除标志 */
    if (smbus_error) {
        smbus_error = 0;
        return HAL_ERROR;
    }
    return HAL_OK;
}

/**
 * @brief 写入16位寄存器 (中断模式，同步等待)
 */
static HAL_StatusTypeDef INA238_WriteReg_IT(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                            uint8_t reg_addr, uint16_t reg_data) {
    uint8_t tx_buf[3];
    HAL_StatusTypeDef status;

    /* 主机为小端，INA238 需要大端 (MSB first) */
    tx_buf[0] = reg_addr;
    tx_buf[1] = (reg_data >> 8) & 0xFF;   /* 高字节 */
    tx_buf[2] = reg_data & 0xFF;          /* 低字节 */

    smbus_tx_complete = 0;
    smbus_error = 0;
    status = HAL_SMBUS_Master_Transmit_IT(hsmbus, dev_addr << 1, tx_buf, 3,
                                          SMBUS_FIRST_AND_LAST_FRAME_NO_PEC);
    if (status != HAL_OK) return status;

    return INA238_WaitForFlag(&smbus_tx_complete, INA238_TIMEOUT);
}

/**
 * @brief 读取16位寄存器 (中断模式，同步等待)
 */
static HAL_StatusTypeDef INA238_ReadReg16_IT(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                             uint8_t reg_addr, uint16_t *reg_data) {
    uint8_t rx_buf[2];
    HAL_StatusTypeDef status;

    /* 1. 发送寄存器地址，无停止位 */
    smbus_tx_complete = 0;
    smbus_error = 0;
    status = HAL_SMBUS_Master_Transmit_IT(hsmbus, dev_addr << 1, &reg_addr, 1,
                                          SMBUS_FIRST_FRAME);
    if (status != HAL_OK) return status;
    status = INA238_WaitForFlag(&smbus_tx_complete, INA238_TIMEOUT);
    if (status != HAL_OK) return status;

    /* 2. 接收2字节数据，带停止位 */
    smbus_rx_complete = 0;
    smbus_error = 0;
    status = HAL_SMBUS_Master_Receive_IT(hsmbus, dev_addr << 1, rx_buf, 2,
                                         SMBUS_LAST_FRAME_NO_PEC);
    if (status != HAL_OK) return status;
    status = INA238_WaitForFlag(&smbus_rx_complete, INA238_TIMEOUT);
    if (status == HAL_OK) {
        /* 直接组合：rx_buf[0]为高字节，rx_buf[1]为低字节 */
        *reg_data = ((uint16_t)rx_buf[0] << 8) | rx_buf[1];
    }
    return status;
}

/**
 * @brief 读取24位功率寄存器 (中断模式，同步等待)
 */
static HAL_StatusTypeDef INA238_ReadReg24_IT(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                             uint8_t reg_addr, uint32_t *reg_data) {
    uint8_t rx_buf[3];
    HAL_StatusTypeDef status;

    /* 1. 发送寄存器地址 */
    smbus_tx_complete = 0;
    status = HAL_SMBUS_Master_Transmit_IT(hsmbus, dev_addr << 1, &reg_addr, 1,
                                          SMBUS_FIRST_FRAME);
    if (status != HAL_OK) return status;
    status = INA238_WaitForFlag(&smbus_tx_complete, INA238_TIMEOUT);
    if (status != HAL_OK) return status;

    /* 2. 接收3字节数据 */
    smbus_rx_complete = 0;
    status = HAL_SMBUS_Master_Receive_IT(hsmbus, dev_addr << 1, rx_buf, 3,
                                         SMBUS_LAST_FRAME_NO_PEC);
    if (status != HAL_OK) return status;
    status = INA238_WaitForFlag(&smbus_rx_complete, INA238_TIMEOUT);
    if (status == HAL_OK) {
        *reg_data = ((uint32_t)rx_buf[0] << 16) | ((uint32_t)rx_buf[1] << 8) | rx_buf[2];
    }
    return status;
}

/**
 * @brief 等待 ADC 转换完成 (CNVRF = 1)
 */
static HAL_StatusTypeDef INA238_WaitForConversion(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr) {
    uint16_t diag;
    uint32_t tickstart = HAL_GetTick();

    do {
        if (INA238_ReadReg16_IT(hsmbus, dev_addr, INA238_REG_DIAG_ALRT, &diag) != HAL_OK) {
            return HAL_ERROR;
        }
        if (diag & 0x0002) {  /* CNVRF 位 */
            return HAL_OK;
        }
        if ((HAL_GetTick() - tickstart) > INA238_CONV_TIMEOUT) {
            return HAL_TIMEOUT;
        }
        HAL_Delay(1);
    } while (1);
}

/* -------------------------- 公共函数 -------------------------- */

/**
 * @brief 初始化 INA238 设备
 * @param hsmbus     SMBUS 句柄
 * @param dev_addr   7位设备地址
 * @param r_shunt    分流电阻值 (Ω)
 * @param max_current 最大测量电流 (A) 用于计算 CURRENT_LSB
 */
HAL_StatusTypeDef INA238_Init(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                              float r_shunt, float max_current) {
    HAL_StatusTypeDef status;
    uint16_t shunt_cal;
    float current_lsb;

    /* 1. 配置 CONFIG: ADCRANGE = 0 (±163.84mV) */
    status = INA238_WriteReg_IT(hsmbus, dev_addr, INA238_REG_CONFIG, INA238_CONFIG_VAL);
    if (status != HAL_OK) return status;

    /* 2. 配置 ADC_CONFIG: 连续模式，转换时间 1052μs，不平均 */
    status = INA238_WriteReg_IT(hsmbus, dev_addr, INA238_REG_ADC_CONFIG, INA238_ADC_CONFIG_VAL);
    if (status != HAL_OK) return status;

    /* 3. 计算 SHUNT_CAL */
    current_lsb = max_current / 32768.0f;               /* CURRENT_LSB = 最大电流 / 2^15 */
    shunt_cal = (uint16_t)(819.2e6f * current_lsb * r_shunt);
    status = INA238_WriteReg_IT(hsmbus, dev_addr, INA238_REG_SHUNT_CAL, shunt_cal);
    return status;
}

/**
 * @brief 读取 INA238 所有测量数据
 * @param hsmbus     SMBUS 句柄
 * @param dev_addr   7位设备地址
 * @param data       输出数据结构体
 * @param current_lsb 预先计算的 CURRENT_LSB 值 (应与 Init 时一致)
 */
HAL_StatusTypeDef INA238_ReadAllData(SMBUS_HandleTypeDef *hsmbus, uint8_t dev_addr,
                                     INA238_DataTypeDef *data, float current_lsb) {
    uint16_t reg16;
    uint32_t reg24;

    if (data == NULL) return HAL_ERROR;
    memset(data, 0, sizeof(INA238_DataTypeDef));

    /* 等待转换完成 (确保数据是最新的) */
    if (INA238_WaitForConversion(hsmbus, dev_addr) != HAL_OK) {
        data->status = HAL_TIMEOUT;
        return HAL_TIMEOUT;
    }

    /* 1. 分流电压 (mV) */
    if (INA238_ReadReg16_IT(hsmbus, dev_addr, INA238_REG_VSHUNT, &reg16) != HAL_OK) goto err;
    data->vshunt_mv = (int16_t)reg16 * 0.005f;   /* 5μV/LSB -> mV */

    /* 2. 总线电压 (V) */
    if (INA238_ReadReg16_IT(hsmbus, dev_addr, INA238_REG_VBUS, &reg16) != HAL_OK) goto err;
    data->vbus_v = reg16 * 0.003125f;            /* 3.125mV/LSB -> V */

    /* 3. 电流 (A) */
    if (INA238_ReadReg16_IT(hsmbus, dev_addr, INA238_REG_CURRENT, &reg16) != HAL_OK) goto err;
    data->current_a = (int16_t)reg16 * current_lsb;

    /* 4. 功率 (W) */
    if (INA238_ReadReg24_IT(hsmbus, dev_addr, INA238_REG_POWER, &reg24) != HAL_OK) goto err;
    data->power_w = reg24 * current_lsb * 0.2f;

    /* 5. 温度 (℃) */
    if (INA238_ReadReg16_IT(hsmbus, dev_addr, INA238_REG_DIETEMP, &reg16) != HAL_OK) goto err;
    data->temp_c = (int16_t)(reg16 >> 4) * 0.125f; /* 125m°C/LSB */

    /* 6. 厂商ID / 设备ID (通信验证) */
    if (INA238_ReadReg16_IT(hsmbus, dev_addr, INA238_REG_MANUFACTURER, &data->mfr_id) != HAL_OK) goto err;
    if (INA238_ReadReg16_IT(hsmbus, dev_addr, INA238_REG_DEVICE_ID, &data->dev_id) != HAL_OK) goto err;

    data->status = HAL_OK;
    return HAL_OK;

err:
    data->status = HAL_ERROR;
    return HAL_ERROR;
}

/**
 * @brief 批量读取总线上的所有 INA238 设备
 * @param hsmbus     SMBUS 句柄
 * @param dev_addrs  设备地址数组
 * @param dev_num    设备数量
 * @param all_data   输出数据数组 (长度需 >= dev_num)
 * @param r_shunt    分流电阻 (所有设备相同，或可根据索引定制)
 * @param max_current 最大电流 (所有设备相同)
 */
void INA238_ReadBusDevices(SMBUS_HandleTypeDef *hsmbus, const uint8_t *dev_addrs, uint8_t dev_num,
                           INA238_DataTypeDef *all_data, float r_shunt, float max_current) {
    float current_lsb = max_current / 32768.0f;

    for (uint8_t i = 0; i < dev_num; i++) {
        
        /* 读取数据 */
        INA238_ReadAllData(hsmbus, dev_addrs[i], &all_data[i], current_lsb);

        /* 打印结果 (需实现 printf 重定向) */
        if (all_data[i].status == HAL_OK) {
            printf("BUS%d Addr 0x%02X: Vshunt=%.2fmV, Vbus=%.2fV, I=%.3fA, P=%.2fW, T=%.1f°C, MFR=0x%04X, DEV=0x%04X\r\n",
                   (hsmbus->Instance == I2C1) ? 1 : 2,
                   dev_addrs[i],
                   all_data[i].vshunt_mv,
                   all_data[i].vbus_v,
                   all_data[i].current_a,
                   all_data[i].power_w,
                   all_data[i].temp_c,
                   all_data[i].mfr_id,
                   all_data[i].dev_id);
        } else {
            printf("BUS%d Addr 0x%02X: Read failed (status=%d)\r\n",
                   (hsmbus->Instance == I2C1) ? 1 : 2,
                   dev_addrs[i],
                   all_data[i].status);
        }
    }
}