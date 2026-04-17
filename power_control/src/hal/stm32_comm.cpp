#include <thread>
#include <chrono>
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "log.h"
#include "stm32_comm.h"

void Protocol_PackVoltageConfig(VoltageConfigPacket* pkt, PowerSupplyConfig* cfg) {
    memset(pkt, 0, sizeof(VoltageConfigPacket));
    pkt->cmd_id = CMD_SET_VOLTAGE;
    pkt->device_id = cfg->dac_device;
    pkt->channel = cfg->dac_channel;
    pkt->dac_value = cfg->dac_value;
    pkt->enable_pin = cfg->enable_pin; // 可根据需要设置使能引脚
    pkt->pin_port = cfg->enable_port; // 可根据需要设置使能引脚端口
}

void Protocol_PackPinConfig(PinConfigPacket* pkt, uint8_t port, uint16_t pin, uint8_t level) {
    memset(pkt, 0, sizeof(PinConfigPacket));
    pkt->cmd_id = CMD_SET_PIN;
    pkt->port = port;
    pkt->pin = pin;
    pkt->level = level;
}

void Protocol_PackSampleConfig(SampleConfigPacket* pkt, uint8_t state, uint8_t type) {
    memset(pkt, 0, sizeof(SampleConfigPacket));
    pkt->cmd_id = CMD_START_SAMPLING;
    pkt->state = state;
    pkt->type = type;
}

void Protocol_ParseSampleData(const uint8_t* data, int length, SampleDataPacket* out) {
    if (length < static_cast<int>(sizeof(SampleDataPacket))) {
        LOG_ERROR("Received sample data too short: %d bytes, expected %zu bytes", length, sizeof(SampleDataPacket));
        return;
    }

    memcpy(out, data, sizeof(SampleDataPacket));
    
    // 只在调试模式下打印采样数据，避免刷屏
    // switch (out->type) {
    //     case I2C_DATA_VBUS:
    //         LOG_INFO("  I2C_DATA_VBUS: ");
    //         break;
    //     case I2C_DATA_CURRENT:
    //         LOG_INFO("  I2C_DATA_CURRENT: ");
    //         break;
    //     default:
    //         LOG_INFO("  Unknown type %d: ", out->type);
    //         break;
    // }
    // LOG_INFO("\n");
    // for (int i = 0; i < 13; ++i) {
    //         LOG_INFO("%.8f ", out->values[i]);
    // }           
    // LOG_INFO("\n");
    // for (int i = 13; i < 27; ++i) {
    //         LOG_INFO("%.8f ", out->values[i]);
    // }    
    // LOG_INFO("\n");
}

void SendVoltageConfig(USBDriver& dev, PowerSupplyConfig* cfg) {
    VoltageConfigPacket pkt;
    Protocol_PackVoltageConfig(&pkt, cfg);
    dev.send(reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));
}

void SendPinConfig(USBDriver& dev, int port, int pin, int level) {
    PinConfigPacket pin_cfg;
    Protocol_PackPinConfig(&pin_cfg, port, pin, level);
    dev.send(reinterpret_cast<uint8_t*>(&pin_cfg), sizeof(pin_cfg));
}

void SendSampleConfig(USBDriver& dev, int state, int type) {
    SampleConfigPacket sample_cfg;
    Protocol_PackSampleConfig(&sample_cfg, state, type);
    dev.send(reinterpret_cast<uint8_t*>(&sample_cfg), sizeof(sample_cfg));
}

void SendStartSample(USBDriver& dev, int type) {
    SendSampleConfig(dev, SAMPLE_STATE_START, type);
}

void SendStopSample(USBDriver& dev, int type) {
    SendSampleConfig(dev, SAMPLE_STATE_STOP, type);
}

void SendPowerOn(USBDriver& dev, PowersConfig* cfg) {
    SequenceConfigPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.cmd_id = CMD_POWER_ON;
    for(int i = 0; i < POWER_SUPPLY_COUNT; ++i) {
        pkt.sequence [i] = cfg->sequences[i].sequence;
        pkt.interval_ms[i] = cfg->sequences[i].interval_ms;
        LOG_INFO("SequenceConfig: id=%d, sequence=%d, interval_ms=%d", i, cfg->sequences[i].sequence, cfg->sequences[i].interval_ms);
    }
    dev.send(reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));
}

void SendPowerOff(USBDriver& dev, PowersConfig* cfg) {
    uint8_t cmd[1] = {CMD_POWER_OFF};
    dev.send(cmd, sizeof(cmd));
}

float GetActualVoltage(USBDriver& dev, int means_pt) {
    int idx = means_pt;
    if (idx < 0 || idx >= SAMPLE_DATA_COUNT) {
        LOG_ERROR("Invalid global index %d for power means_pt ID %d", idx, means_pt);
        return -1.0f;
    }

    // Start sampling voltage data
    SendStartSample(dev, SAMPLE_TYPE_VOLTAGE);
    // wait stm32 to sample data, so need to sleep
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // get voltage from stm32
    uint8_t buffer[USB_REPORT_SIZE];
    float voltages[SAMPLE_DATA_COUNT] = {0.0f};
    int bytes = dev.receive(buffer, USB_REPORT_SIZE);
    SampleDataPacket pkt;
    Protocol_ParseSampleData(buffer, bytes, &pkt);
    if (pkt.type == I2C_DATA_VBUS) {
        memcpy(voltages, pkt.channel_volt_mv, sizeof(voltages));
    } else {
        LOG_ERROR("Received sample data with unexpected type %d", pkt.type);
        std::abort();
        return -1.0f;
    }
    LOG_INFO("Voltage data[%d]=%.4f", idx, voltages[idx]);

    // Stop sampling
    SendStopSample(dev, SAMPLE_TYPE_VOLTAGE);
    
    return voltages[idx];
}