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

void Protocol_PackVoltageConfig(VoltageConfigPacket* pkt, uint8_t channel, uint16_t voltage_mv, uint16_t current_ma) {
    memset(pkt, 0, sizeof(VoltageConfigPacket));
    pkt->cmd_id = 0x01;
    pkt->channel = channel;
    pkt->voltage_mv = voltage_mv;
    pkt->current_ma = current_ma;
}

void Protocol_ParseSampleData(const uint8_t* data, int length, SampleDataPacket* out) {
    if (length >= static_cast<int>(sizeof(SampleDataPacket))) {
        memcpy(out, data, sizeof(SampleDataPacket));
    }
}