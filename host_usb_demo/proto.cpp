/*
 * Copyright (C) 2024 - FlyingChip Technology (Shanghai) Co., Ltd. All rights reserved.
 *
 * This file contains information that is proprietary to FlyingChip.
 * The holder of this file shall treat all information contained herein as
 * confidential, use the information only for its intended purpose, illustrate
 * the copyright of FlyingChip and not duplicate, disclose, modificate or disseminate
 * any of this information in any manner unless FlyingChip has otherwise
 * provided express written permission.
 * Use of the file may require a license of intellectual property from FlyingChip.
 * This file conveys no express or implied licenses to any intellectual property
 * rights belonging to FlyingChip.
 *
 * ALL INFORMATION CONTAINED IN THIS FILE IS FURNISHED “AS IS”.
 * FLYINGCHIP DISCLAIMS ANY AND ALL TYPES OF WARRANTIES, EXPRESS, IMPLIED, OR
 * STATUTORY, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY OR FITNESS FOR
 * A PARTICULAR PURPOSE WITH RESPECT TO THE INFORMATION PROVIDE HEREUNDER.
 *
 * FLYINGCHIP RESERVES ALL RIGHTS NOT EXPRESSLY GRANTED TO YOU HEREUNDER.
 */
#include "proto.h"
#include <cstring>
#include <cstdio>

void Protocol_PackVoltageConfig(VoltageConfigPacket* pkt,uint8_t device_id ,uint8_t channel, uint16_t voltage_mv) {
    memset(pkt, 0, sizeof(VoltageConfigPacket));
    pkt->cmd_id = 0x01;
    pkt->device_id = device_id;
    pkt->channel = channel;
    pkt->voltage_mv = voltage_mv;
}

void Protocol_PackPinConfig(PinConfigPacket* pkt, uint8_t port, uint16_t pin, uint8_t level) {
    memset(pkt, 0, sizeof(PinConfigPacket));
    pkt->cmd_id = 0x02;
    pkt->port = port;
    pkt->pin = pin;
    pkt->level = level;
}

void Protocol_ParseSampleData(const uint8_t* data, int length, SampleDataPacket* out) {
    if (length >= static_cast<int>(sizeof(SampleDataPacket))) {
        memcpy(out, data, sizeof(SampleDataPacket));
    }
    
    switch (out->type) {
        case I2C_DATA_VBUS:
            printf("  I2C_DATA_VBUS: ");
            break;
        case I2C_DATA_CURRENT:
            printf("  I2C_DATA_CURRENT: ");
            break;
        default:
            printf("  Unknown type %d: ", out->type);
            break;
    }
    printf("\n");
    for (int i = 0; i < 13; ++i) {
            printf("%.8f ", out->i2c_data[i]);
    }           
    printf("\n");
    for (int i = 13; i < 27; ++i) {
            printf("%.8f ", out->i2c_data[i]);
    }    
    printf("\n");
}