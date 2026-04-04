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

#include "user_callback.h"
#include "stm32h5xx_hal.h"

extern SPI_HandleTypeDef hspi1;

/* 全局传输完成标志（volatile 供多个文件访问） */
volatile uint8_t smbus_tx_complete = 0;
volatile uint8_t smbus_rx_complete = 0;
volatile uint8_t smbus_error = 0;

/* 静态全局变量，保存当前注册的回调 */
static DAC_SPI_Callbacks_t* g_dac_spi_cb = NULL;

/* 注册回调函数 */
void DAC_SPI_Callback_Register(DAC_SPI_Callbacks_t* cb) {
    if (cb == NULL) {
        return;
    }
    g_dac_spi_cb = cb;
}

/**
 * @brief SPI传输完成回调（由HAL库在DMA传输完成后调用）
 * @param hspi SPI句柄
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    // 仅处理SPI1，且已注册回调
    if (hspi == &hspi1 && g_dac_spi_cb != NULL && g_dac_spi_cb->TxCpltCallback != NULL) {
        g_dac_spi_cb->TxCpltCallback(g_dac_spi_cb->TxCpltArg);
    }
}

/**
 * @brief SPI错误回调
 * @param hspi SPI句柄
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1 && g_dac_spi_cb != NULL && g_dac_spi_cb->ErrorCallback != NULL) {
        g_dac_spi_cb->ErrorCallback(g_dac_spi_cb->ErrorArg);
    }
}

void HAL_SMBUS_MasterTxCpltCallback(SMBUS_HandleTypeDef *hsmbus) {
    smbus_tx_complete = 1;
}

void HAL_SMBUS_MasterRxCpltCallback(SMBUS_HandleTypeDef *hsmbus) {
    smbus_rx_complete = 1;
}

void HAL_SMBUS_ErrorCallback(SMBUS_HandleTypeDef *hsmbus) {
    smbus_error = 1;
    smbus_tx_complete = 1;   /* 避免死等 */
    smbus_rx_complete = 1;
}