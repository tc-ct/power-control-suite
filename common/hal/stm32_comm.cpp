#include <thread>
#include <chrono>
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "log.h"
#include "adc_def.h"
#include "stm32_comm.h"

namespace {
constexpr int kRawToDisplaySampleId[SAMPLE_DATA_COUNT] = {
    8, 7, 9, 6, 19, 13, 14, 0, 1, 2, 17, 18, 16, 12, 11, 15, 3, 5, 20, 21, 22, 23, 24, 25, 26, 4, 10
};

float GetIna238MaxCurrentForRawIndex(const PowersConfig* config, uint8_t raw_index) {
    const float fallback = (raw_index < I2C1_INA238_NUM) ? MAX_CURRENT1 : MAX_CURRENT2;
    if (config == nullptr) {
        return fallback;
    }
    if (raw_index >= SAMPLE_DATA_COUNT) {
        return fallback;
    }

    const int sample_id = kRawToDisplaySampleId[raw_index];
    if (sample_id < 0 || sample_id >= SAMPLE_DATA_COUNT) {
        return fallback;
    }

    const float configured = config->sample_cfg[sample_id].max_current_a;
    return configured > 0.0f ? configured : fallback;
}
}

void Protocol_PackVoltageConfig(VoltageConfigPacket* pkt, PowerSupplyConfig* cfg) {
    memset(pkt, 0, sizeof(VoltageConfigPacket));
    pkt->cmd_id = CMD_SET_VOLTAGE;
    pkt->device_id = cfg->dac_device;
    pkt->channel = cfg->dac_channel;
    pkt->dac_value = cfg->dac_value;
    pkt->enable_pin = cfg->enable_pin; 
    pkt->pin_port = cfg->enable_port; 
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

void ProProtocol_PackSampleOnceConfig(SampleConfigPacket* pkt) {
    memset(pkt, 0, sizeof(SampleConfigPacket));
    pkt->cmd_id = CMD_SAMPLING_ONCE;
}

void Protocol_ParseSampleData(const uint8_t* data, int length, SampleDataPacketTF_t* out, const PowersConfig* config) {
    SampleDataPacket* pkt = reinterpret_cast<SampleDataPacket*>(const_cast<uint8_t*>(data));
    if (length < static_cast<int>(sizeof(SampleDataPacket))) {
        LOG_ERROR("Received sample data too short: %d bytes, expected %zu bytes", length, sizeof(SampleDataPacket));
        return;
    }
    out->timestamp = pkt->timestamp;

    // Parse INA238 current registers (raw index 0~25): A/LSB -> mA.
    for (uint8_t raw_index = 0; raw_index < SAMPLE_DATA_COUNT - 1; ++raw_index) {
        const float max_current = GetIna238MaxCurrentForRawIndex(config, raw_index);
        out->channel_curr_ma[raw_index] =
            static_cast<int16_t>(pkt->channel_curr_reg[raw_index]) * INA238_CURRENT_LSB(max_current) * 1000.0f;
    }
    // Parse INA260 current register (raw index 26): fixed LSB.
    out->channel_curr_ma[SAMPLE_DATA_COUNT - 1] =
        static_cast<int16_t>(pkt->channel_curr_reg[SAMPLE_DATA_COUNT - 1]) * INA260_CURRENT_LSB * 1000.0f;

    // for process raw voltage data
    for (uint8_t i = 0; i < SAMPLE_DATA_COUNT - 1; i++) {
        out->channel_volt_mv[i] = pkt->channel_volt_reg[i] * INA238_VBUS_LSB_V * 1000.0f;   /* V -> mV */
        // LOG_INFO("Voltage data[%d]=%.4f", i, out->channel_volt_mv[i]);
        // LOG_INFO("Voltage Raw data[%d]=%d ", i, pkt->channel_volt_reg[i]);
    }
    out->channel_volt_mv[SAMPLE_DATA_COUNT - 1] = (pkt->channel_volt_reg[SAMPLE_DATA_COUNT - 1] & 0x7FFF) * INA260_BUSVOLTAGE_LSB * 1000.0f;
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

void SendSampleOnceConfig(USBDriver& dev) {
    SampleConfigPacket sample_cfg;
    ProProtocol_PackSampleOnceConfig(&sample_cfg);
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

    // get voltage from stm32 (parsed values are in mV)
    uint8_t buffer[USB_REPORT_SIZE];
    int bytes = dev.receive(buffer, USB_REPORT_SIZE);
    SampleDataPacket pkt;
    SampleDataPacketTF pk_tf;
    Protocol_ParseSampleData(buffer, bytes, &pk_tf);

    // memcpy(voltages, pkt.channel_volt_reg, sizeof(voltages));
    // LOG_INFO("Voltage data[%d]=%.4f mV", idx, pk_tf.channel_volt_mv[idx]);

    // Stop sampling
    SendStopSample(dev, SAMPLE_TYPE_VOLTAGE);
    
    // GetActualVoltage returns volts.
    return pk_tf.channel_volt_mv[idx] / 1000.0f;
}

