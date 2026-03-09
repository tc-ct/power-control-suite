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

#include "dac_driver.h"
#include "user_callback.h"   

extern SPI_HandleTypeDef hspi1;

/* 内部设备注册表（静态全局，仅驱动内部使用） */
#define MAX_DAC_DEVICES 3
static DAC_HandleTypeDef* dac_devices[MAX_DAC_DEVICES] = {0};

/*=========================== 静态函数实现 ===========================*/

/* 设备特定的传输完成处理函数（静态，供回调使用） */
static void DAC_DeviceTxComplete(void* arg) {
    DAC_HandleTypeDef* hdac = (DAC_HandleTypeDef*)arg;
    // 拉高片选
    HAL_GPIO_WritePin(hdac->cs_port, hdac->cs_pin, GPIO_PIN_SET);
    hdac->tx_complete = 1;
}

/* 设备特定的错误处理函数（静态，供回调使用） */
static void DAC_DeviceError(void* arg) {
    DAC_HandleTypeDef* hdac = (DAC_HandleTypeDef*)arg;
    // 错误处理：拉高片选，恢复标志
    HAL_GPIO_WritePin(hdac->cs_port, hdac->cs_pin, GPIO_PIN_SET);
    hdac->tx_complete = 1;
}

/**
 * @brief  将设备加入内部注册表
 * @param  hdac DAC 句柄
 * @retval HAL_OK   成功
 * @retval HAL_ERROR 注册表已满
 */
static HAL_StatusTypeDef DAC_AddDevice(DAC_HandleTypeDef* hdac) {
    for (int i = 0; i < MAX_DAC_DEVICES; i++) {
        if (dac_devices[i] == NULL) {
            dac_devices[i] = hdac;
            return HAL_OK;
        }
    }
    return HAL_ERROR;
}

/* ---------- DAC7568 专用函数 ---------- */
/**
 * @brief  构造 DAC7568 的 32 位命令字 (12位数据左对齐到 DB19-DB4)
 * @param  channel 通道号 0~7
 * @param  value   12位电压码值 0~4095
 * @param  data    输出缓冲区 (4字节, 大端序)
 * @note   命令格式: 前缀[31:28]=0, 控制[27:24]=0011(写并更新), 
 *         地址[23:20]=channel, 数据[19:4]=value<<4, 低位忽略
 */
static void DAC7568_Encode(uint8_t channel, uint16_t value,uint8_t* data)
{
    uint32_t cmd = 0;
    cmd |= (DAC7568_CMD_WRITE_UPDATE_ALL << 24);  // DB27-DB24 = 0011
    cmd |= ((channel & 0x0F) << 20);               // DB23-DB20 = 通道地址
    cmd |= ((value & 0x0FFF) << 8);               // DB19-DB8  = 12位数据
    /* 构造32位命令并拆分为4字节 */
    data[0] = (cmd >> 24) & 0xFF;
    data[1] = (cmd >> 16) & 0xFF;
    data[2] = (cmd >> 8) & 0xFF;
    data[3] = cmd & 0xFF;
}

/* ---------- DAC7563 专用函数 ---------- */
/**
 * @brief  构造 DAC7563 的 24 位命令字 (12位数据左对齐到 DB15-DB4)
 * @param  channel 通道号 0~1
 * @param  value   12位电压码值 0~4095
 * @param  data    输出缓冲区 (3字节, 大端序)
 * @note   命令格式: 保留[23:22]=0, 命令[21:19]=011(写并更新),
 *         地址[18:16]=channel, 数据[15:4]=value<<4, 低位忽略
 */
static void DAC7563_Encode(uint8_t channel, uint16_t value,uint8_t* data)
{
    /* 构造24位命令：命令=011（写输入并更新DAC） */
    uint32_t cmd = 0;
    cmd |= (DAC7563_CMD_WRITE_UPDATE_ALL << 19);   // DB21-DB19 = 011
    cmd |= ((channel & 0x01) << 16);               // DB18-DB16 = 通道数据
    cmd |= ((value & 0x0FFF) << 4);                // 12位数据左对齐
    /* 构造24位命令并拆分为3字节 */
    data[0] = (cmd >> 16) & 0xFF;
    data[1] = (cmd >> 8) & 0xFF;
    data[2] = cmd & 0xFF;
}

/**
 * @brief  等待传输完成标志，带超时处理
 * @param  hdac        DAC设备句柄
 * @param  timeout_ms  超时时间（毫秒）
 * @retval HAL_OK      标志已置位
 * @retval HAL_TIMEOUT 超时，已停止DMA并恢复片选
 */
static HAL_StatusTypeDef DAC_WaitForFlag(DAC_HandleTypeDef* hdac, uint32_t timeout_ms) {
    uint32_t tickstart = HAL_GetTick();
    while (hdac->tx_complete == 0) {
        if ((HAL_GetTick() - tickstart) > timeout_ms) {
            // 超时，停止 DMA 并恢复片选
            HAL_SPI_DMAStop(hdac->hspi);
            HAL_GPIO_WritePin(hdac->cs_port, hdac->cs_pin, GPIO_PIN_SET);
            hdac->tx_complete = 1;
            return HAL_TIMEOUT;
        }
    }
    return HAL_OK;
}

/*=========================== 全局函数实现 ===========================*/

/**
 * @brief  初始化 DAC 设备
 * @param  hdac     设备句柄
 * @param  type     芯片类型
 * @param  cs_port  片选端口
 * @param  cs_pin   片选引脚
 * @param  hspi     SPI 句柄
 * @retval HAL_OK   成功
 * @retval HAL_ERROR 失败
 */
HAL_StatusTypeDef DAC_Init(DAC_HandleTypeDef* hdac, DAC_Device_TypeDef type,
                            GPIO_TypeDef* cs_port, uint16_t cs_pin,
                            SPI_HandleTypeDef* hspi) {
    if (hdac == NULL || hspi == NULL) return HAL_ERROR;

    hdac->type = type;
    hdac->cs_port = cs_port;
    hdac->cs_pin = cs_pin;
    hdac->hspi = hspi;
    hdac->tx_complete = 1;

    // 片选引脚默认为高电平（取消选中）
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

    // 将设备加入注册表
    return DAC_AddDevice(hdac);
}

/**
 * @brief  设置DAC输出电压（24位命令，12位数据左对齐到DB15-DB4）
 * @param  chip   芯片选择：=SS0(PA2),2=SS1(PA2),2=SS2(PA2)
 * @param  channel 通道：0=DAC-A，1=DAC-B,...
 * @param  value   12位电压值 0~4095
 * @retval HAL_OK / HAL_BUSY / HAL_TIMEOUT / HAL_ERROR
 */
HAL_StatusTypeDef DAC_SetVoltage(DAC_HandleTypeDef* hdac, uint8_t channel, uint16_t value)
{
    if (hdac == NULL) return HAL_ERROR;
    if (!hdac->tx_complete) return HAL_BUSY;

    uint16_t data_size = 0;
    DAC_SPI_Callbacks_t cb;

    // 根据类型格式化数据到hdac->tx_buf
    if (hdac->type == DAC_TYPE_7568) {
        if (channel > 7) return HAL_ERROR;
        DAC7568_Encode(channel, value, hdac->tx_buf);
        data_size = 4;
    } else if (hdac->type == DAC_TYPE_7563) {
        if (channel > 1) return HAL_ERROR;
        DAC7563_Encode(channel, value, hdac->tx_buf);
        data_size = 3;
    } else {
        return HAL_ERROR;
    }
    // 准备回调结构体：将设备特定的处理函数和当前设备句柄绑定
    cb.TxCpltCallback = DAC_DeviceTxComplete;
    cb.TxCpltArg = (void*)hdac;
    cb.ErrorCallback = DAC_DeviceError;
    cb.ErrorArg = (void*)hdac;

    // 注册到user_callback（临界保护，防止中断干扰）
    __disable_irq();
    DAC_SPI_Callback_Register(&cb);
    __enable_irq();

    // 拉低片选，启动DMA传输（发送+接收）
    hdac->tx_complete = 0;
    HAL_GPIO_WritePin(hdac->cs_port, hdac->cs_pin, GPIO_PIN_RESET);

    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive_DMA(hdac->hspi, hdac->tx_buf, hdac->rx_buf, data_size);
    if (status != HAL_OK) {
        // 传输启动失败，恢复状态,拉高电平
        HAL_GPIO_WritePin(hdac->cs_port, hdac->cs_pin, GPIO_PIN_SET);
        hdac->tx_complete = 1;
        return status;
    }
    // 等待传输完成或超时
    return DAC_WaitForFlag(hdac, SPI_TIMEOUT);
}

