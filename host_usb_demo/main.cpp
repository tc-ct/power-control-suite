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
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <Windows.h>

#include "log.h"
#include "usb_driver.h"
#include "proto.h"

// 设备 VID/PID
constexpr uint16_t STM32_VID = 0x0483;
constexpr uint16_t STM32_PID = 0x5750;  // 根据实际修改

// 全局校准状态
static std::mutex g_mutex;
static std::condition_variable g_cv;
static bool g_voltage_received = false;
static bool g_calibration_enabled = false;
static float g_last_voltages[27] = {0.0f};


// 电源 ID 到采样数据索引的映射（0-13 为 INA238 通道，超出则无法自动校准）
struct MeasurementPoint {
    int global_index;      // 通道索引
};

// 为每个电源 ID (0~17) 定义测量点列表
static std::vector<MeasurementPoint> g_power_measurements[18] = {
    // ID 0: VDDIO1833_B0_MCU        
    { {7} },
    // ID 1: VDDIO1833_B1_MCU        
    { {8} },
    // ID 2: VDDIO1833_B2_MCU        
    { {9} },
    // ID 3: VDDIO1833_B0_MAIN       
    { {16} },
    // ID 4: VDDIO1833_B1_MAIN     
    { {25} },
    // ID 5: VDDIO1833_B2_MAIN       
    { {17} },
    // ID 6: VCC_OV8_AO             
    { {1} },
    // ID 7: VCC_OV8_MCU        
    { {0} },
    // ID 8: VCC_OV8_MAIN             
    { {26}},
    // ID 9: VDD_CPU                  
    { {13} },
    // ID 10: VDDIO18_AO              
    { {5} },
    // ID 11: VDDIO18_MCU             
    { {6} },
    // ID 12: VDDIO18_MAIN            
    { {15} },
    // ID 13: AVDD_PLL_AO             
    { {12} },
    // ID 14: VCC_1V8_MCU             
    { {10} },
    // ID 15: VCCA_1V8_MAIN           
    { {20} },
    // ID 16: VDDQL_DDR               
    { {23} },
    // ID 17: VDDQ_DDR                
    { {24} }
};

// 接收数据回调函数
void onDataReceived(const uint8_t* data, int length) {
    if (length >= sizeof(SampleDataPacket)) {
        SampleDataPacket pkt;
        Protocol_ParseSampleData(data, length, &pkt);
        std::lock_guard<std::mutex> lock(g_mutex);
        if (pkt.type == I2C_DATA_VBUS) {
            memcpy(g_last_voltages, pkt.i2c_data, sizeof(g_last_voltages));
            g_voltage_received = true;
            g_cv.notify_one();
        } 
    }else {
        // 原始数据打印
        printf("  Raw: ");
        for (int i = 0; i < length; ++i) {
            printf("%02X ", data[i]);
        }
        printf("\n");
    }
}

int channelLetterToIndex(char ch) {
    ch = toupper(ch);
    if (ch >= 'A' && ch <= 'H') {
        return ch - 'A';   // A=0, B=1, ..., H=7
    }
    return -1;
}

int portLetterToIndex(char ch) {
    ch = toupper(ch);
    if (ch == 'B') return 0;
    if (ch == 'C') return 1;
    return -1;
}

int sampleTypeToValue(const char* typeStr) {
    if (strcmp(typeStr, "vol") == 0 || strcmp(typeStr, "voltage") == 0) return 4;
    if (strcmp(typeStr, "cur") == 0 || strcmp(typeStr, "current") == 0) return 5;
    return -1;
}

// ==================== 电源配置结构体 ====================
struct PowerSupplyConfig {
    float R1;
    float R2;
    float R3;
    int dac_device;
    int dac_channel;
    float Vfb;
    float Vmax;
    int enable_port;
    int enable_pin;
};

// 18路电源配置数组 (需根据实际硬件修改)
static PowerSupplyConfig g_power_supplies[18] = {
    {32.4f, 4.7f, 14.7f, 0, 6, 0.6f, 2.5f, 1, 6},
    {32.4f, 4.7f, 14.7f, 0, 7, 0.6f, 2.5f, 1, 7},
    {32.4f, 4.7f, 14.7f, 1, 0, 0.6f, 2.5f, 1, 8},
    {32.4f, 4.7f, 14.7f, 1, 5, 0.6f, 2.5f, 1, 9},
    {32.4f, 4.7f, 14.7f, 1, 6, 0.6f, 2.5f, 1, 10},
    {32.4f, 4.7f, 14.7f, 1, 7, 0.6f, 2.5f, 1, 11},    //5
    {24.0f, 20.0f, 30.0f, 0, 1, 0.6f, 2.5f, 1, 1},
    {24.0f, 20.0f, 30.0f, 0, 4, 0.6f, 2.5f, 1, 3},
    {24.0f, 20.0f, 30.0f, 1, 2, 0.6f, 2.5f, 1, 12},
    {24.0f, 20.0f, 30.0f, 1, 1, 0.6f, 2.5f, 1, 4},
    {32.4f, 4.7f, 14.7f, 0, 2, 0.6f, 2.5f, 1, 0},    //10
    {32.4f, 4.7f, 14.7f, 0, 5, 0.6f, 2.5f, 1, 2},
    {32.4f, 4.7f, 14.7f, 1, 3, 0.6f, 2.5f, 1, 5},
    {24.3f, 4.64f, 13.9f, 0, 0, 0.5f, 2.5f, 1, 13},     //13
    {24.3f, 4.64f, 13.9f, 0, 3, 0.5f, 2.5f, 0, 0},
    {24.3f, 4.64f, 13.9f, 1, 4, 0.5f, 2.5f, 0, 1},
    {10.0f, 8.25f, 15.0f, 2, 0, 0.5f, 1.25f, 0, 15},
    {24.0f, 10.0f, 30.0f, 2, 1, 0.5f, 1.25f, 0, 14}  
};

// 根据电源ID和期望电压计算DAC寄存器值（0-4095）
int CalculateDACValue(int id, float target_voltage) {
    if (id < 0 || id >= 18) {
        LOG_ERROR("Invalid power supply ID: %d", id);
        return 0;
    }
    const auto& cfg = g_power_supplies[id];
    float Vfb = cfg.Vfb;
    float R1 = cfg.R1;
    float R2 = cfg.R2;
    float R3 = cfg.R3;
    float Vmax = cfg.Vmax;

    float term1 = Vfb;
    float term2 = target_voltage - Vfb * (1 + R1 / R2);
    float term3 = term2 * R3 / R1;
    float Vdac = term1 - term3;

    if (Vdac < 0) Vdac = 0;
    if (Vdac > Vmax) Vdac = Vmax;

    return static_cast<int>(Vdac / Vmax * 4096.0f);
}

// 发送 DAC 配置命令
void SendDACConfig(USBDriver& dev, int device, int channel, int dac) {
    VoltageConfigPacket cfg;
    Protocol_PackVoltageConfig(&cfg, device, channel, dac);
    dev.send(reinterpret_cast<uint8_t*>(&cfg), sizeof(cfg));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// 发送引脚配置命令
void SendPinConfig(USBDriver& dev, int port, int pin, int level) {
    PinConfigPacket pin_cfg;
    Protocol_PackPinConfig(&pin_cfg, port, pin, level);
    dev.send(reinterpret_cast<uint8_t*>(&pin_cfg), sizeof(pin_cfg));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// 获取实际电压（通过采样）
float GetActualVoltage(USBDriver& dev, int power_id) {
    const auto& points = g_power_measurements[power_id];
    if (points.empty()) {
        LOG_ERROR("No measurement point for power ID %d", power_id);
        return -1.0f;
    }
    int idx = points[0].global_index;   // 取第一个测量点的全局索引
    if (idx < 0 || idx >= 27) {
        LOG_ERROR("Invalid global index %d for power ID %d", idx, power_id);
        return -1.0f;
    }
    // 启动电压采样
    uint8_t start_cmd_i2c[] = {0x03, 0x01, 0x04};
    dev.send(start_cmd_i2c, sizeof(start_cmd_i2c));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 等待数据
    {
        std::unique_lock<std::mutex> lock(g_mutex);
        g_voltage_received = false;
        auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        while (!g_voltage_received) {
            if (g_cv.wait_until(lock, timeout) == std::cv_status::timeout) {
                LOG_ERROR("Timeout waiting for voltage data");
                return -1.0f;
            }
        }
    }

    // 读取对应电压
    float voltage = g_last_voltages[idx];
    // 停止采样
    uint8_t stop_cmd[] = {0x03, 0x00, 0x04};
    dev.send(stop_cmd, sizeof(stop_cmd));

    return voltage;
}

// 读取多次实际电压并返回平均值（次数 = samples）
float ReadAverageVoltage(USBDriver& dev, int power_id, int samples) {
    float sum = 0.0f;
    int valid_count = 0;
    for (int i = 0; i < samples; ++i) {
        float volt = GetActualVoltage(dev, power_id);
        if (volt >= 0) {
            sum += volt;
            ++valid_count;
        }
        // 两次采样之间稍作延时，避免读取过快
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (valid_count == 0) {
        LOG_ERROR("No valid voltage readings for power ID %d", power_id);
        return -1.0f;
    }
    return sum / valid_count;
}

void LogVoltageInfo(USBDriver& dev, int power_id, float target_voltage) {
    // 等待电源稳定（可根据实际调整）
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 读取实际电压
    float actual_voltage = GetActualVoltage(dev, power_id);
    if (actual_voltage < 0) {
        LOG_ERROR("Failed to read actual voltage for power ID %d", power_id);
        return;
    }
    
    float error = actual_voltage - target_voltage;
    LOG_INFO("Power ID %d: target=%.3f V, actual=%.3f V, error=%.3f V",
             power_id, target_voltage, actual_voltage, error);
}

// 带校准的电压设置
void SetVoltageWithCalibration(USBDriver& dev, int power_id, float target_voltage) {
    const auto& cfg = g_power_supplies[power_id];
    int dac_value = CalculateDACValue(power_id, target_voltage);
    int mv = static_cast<int>(dac_value * cfg.Vmax * 1000.0f / 4096.0f);
    LOG_INFO("Setting power ID %d to %.3f V (DAC=%d, mv=%d)", power_id, target_voltage, dac_value, mv);

    // 发送 DAC 和使能引脚
    SendDACConfig(dev, cfg.dac_device, cfg.dac_channel, dac_value);
    SendPinConfig(dev, cfg.enable_port, cfg.enable_pin, 1);  // 使能

    if (!g_calibration_enabled) {
        return;
    }

    // 校准循环
    const int max_iterations = 5;
    float tolerance = target_voltage * 0.007f + 0.003125f;  // 0.7% + 3.125mV
    bool satisfied = false;
    float actual_voltage = 0.0f;

float current_target = target_voltage;   // 当前设定值，逐步调整
for (int iter = 0; iter < max_iterations; ++iter) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    actual_voltage = ReadAverageVoltage(dev, power_id,10); 
    if (actual_voltage < 0) {
        LOG_ERROR("Failed to read voltage, abort calibration");
        break;
    }

    float error = actual_voltage - target_voltage;   // 实际电压与最终目标的偏差
    float abs_error = std::abs(error);
    if(abs_error>2.5f)
    break;
    LOG_INFO("Iteration %d: actual=%.4f V, target=%.4f V, error=%.4f V, tolerance=%.4f V",
             iter+1, actual_voltage, target_voltage, error, tolerance);

    if (abs_error <= tolerance) {
        satisfied = true;
    } else satisfied = false;

    // 补偿：基于当前设定值调整
    float compensated_target = current_target + (target_voltage - actual_voltage);
    // 或使用比例增益 K，例如：compensated_target = current_target + K * error
    // 限制补偿范围
    if (compensated_target < 0) compensated_target = 0;
 //   if (compensated_target > cfg.Vmax) compensated_target = cfg.Vmax;  // 根据实际电源范围

    // 更新当前设定值
    current_target = compensated_target;

    int new_dac = CalculateDACValue(power_id, current_target);
    int new_mv = static_cast<int>(new_dac * cfg.Vmax * 1000.0f / 4096.0f);
    LOG_INFO("Compensating: new target=%.4f V -> DAC=%d, mv=%d", current_target, new_dac, new_mv);
    SendDACConfig(dev, cfg.dac_device, cfg.dac_channel, new_dac);
}

    // 最终打印结果
    if (satisfied) {
        LOG_INFO("Calibration successful: Power: %d , actual=%.4f, Vtarget=%.4f V", power_id,actual_voltage,target_voltage);
    } else {
        LOG_WARN("Calibration did not meet tolerance after %d iterations: target=%.4f V, actual=%.4f V",
                 max_iterations, target_voltage, actual_voltage);
    }
}

// 打印帮助信息
void printHelp(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --pid <hex>          Set device PID (hex), default 0x5750\n");
    printf("  --dac <dev> <ch> <mv>  Configure DAC: dev=0/1, ch=A-H, mv=0-3300\n");
    printf("  --pin <port> <pin> <level>  Configure GPIO: port=B/C, pin=0-15, level=0/1\n");
    printf("  --start-sampling <type>  Start sampling (type=vol/cur)\n");
    printf("  --stop-sampling <type>   Stop sampling (type=vol/cur)\n");
    printf("  --sample <type> <sec>    Start sampling, wait for sec, then stop\n");
    printf("  --volt <id> <voltage>    Set power supply voltage (id 0-17, voltage in V)\n");
    printf("  --all-off                Turn off all power supplies (set DAC to 0 and disable pins)\n");
    printf("  --cal-on                 Enable calibration mode\n");
    printf("  --cal-off                Disable calibration mode\n");
    printf("  --help                   Show this help\n");
    printf("\nExamples:\n");
    printf("  %s --dac 1 B 200 --start-sampling vol\n", prog);
    printf("  %s --pin C 3 1 --stop-sampling cur\n", prog);
    printf("  %s --sample vol 10\n", prog);
    printf("  %s --volt 5 1.25 --cal-on\n", prog);
    printf("  %s --all-off\n", prog);
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    log_set_level(LOG_LEVEL_INFO);
    LOG_INFO("STM32 Custom HID Test Program");

    uint16_t pid = STM32_PID;
    bool hasCommand = false;

    enum CmdType {
        CMD_DAC,
        CMD_PIN,
        CMD_SAMPLE,
        CMD_START_SAMPLE,
        CMD_STOP_SAMPLE,
        CMD_VOLT_SET,
        CMD_CAL_ON,
        CMD_CAL_OFF
    };
    struct Command {
        CmdType type;
        int dev;        // 电压设备
        int ch;         // 电压通道
        int dac;         // 电压dac值
        int port;       // 端口
        int pin;        // 引脚号
        int level;      // 电平
        int sample_type; // 采样类型
        int duration;   // 采样时长
        int power_id;   // 电源 ID
        float target_voltage; // 目标电压
    };
    std::vector<Command> commands;

    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
            pid = static_cast<uint16_t>(std::stoi(argv[++i], nullptr, 16));
            hasCommand = true;
        } else if (strcmp(argv[i], "--dac") == 0 && i + 3 < argc) {
            int dev = std::stoi(argv[++i]);
            int ch;
            if (isdigit(argv[i+1][0])) {
                ch = std::stoi(argv[++i]);
            } else {
                ch = channelLetterToIndex(argv[++i][0]);
                if (ch == -1) {
                    LOG_ERROR("Invalid channel letter: %c", argv[i][0]);
                    printHelp(argv[0]);
                    return -1;
                }
            }
            int dac = std::stoi(argv[++i]);
            LOG_INFO("DAC config: dev=%d, ch=%d, dac=%d", dev, ch, dac);
            commands.push_back({CMD_DAC, dev, ch, dac, 0, 0, 0, 0, 0, 0, 0.0f});
            hasCommand = true;
        } else if (strcmp(argv[i], "--pin") == 0 && i + 3 < argc) {
            int port;
            if (isdigit(argv[i+1][0])) {
                port = std::stoi(argv[++i]);
            } else {
                port = portLetterToIndex(argv[++i][0]);
                if (port == -1) {
                    LOG_ERROR("Invalid port letter: %c", argv[i][0]);
                    printHelp(argv[0]);
                    return -1;
                }
            }
            int pin = std::stoi(argv[++i]);
            int level = std::stoi(argv[++i]);
            LOG_INFO("Pin config: port=%d, pin=%d, level=%d", port, pin, level);
            commands.push_back({CMD_PIN, 0, 0, 0, port, pin, level, 0, 0, 0, 0.0f});
            hasCommand = true;
        } else if (strcmp(argv[i], "--sample") == 0 && i + 2 < argc) {
            int sample_type;
            if (isdigit(argv[i+1][0])) {
                sample_type = std::stoi(argv[++i]);
            } else {
                sample_type = sampleTypeToValue(argv[++i]);
                if (sample_type == -1) {
                    LOG_ERROR("Invalid sample type: %s", argv[i]);
                    printHelp(argv[0]);
                    return -1;
                }
            }
            int duration = std::stoi(argv[++i]);
            LOG_INFO("Sample: type=%d, duration=%d sec", sample_type, duration);
            commands.push_back({CMD_SAMPLE, 0, 0, 0, 0, 0, 0, sample_type, duration, 0, 0.0f});
            hasCommand = true;
        } else if (strcmp(argv[i], "--start-sampling") == 0 && i + 1 < argc) {
            int sample_type;
            if (isdigit(argv[i+1][0])) {
                sample_type = std::stoi(argv[++i]);
            } else {
                sample_type = sampleTypeToValue(argv[++i]);
                if (sample_type == -1) {
                    LOG_ERROR("Invalid sample type: %s", argv[i]);
                    printHelp(argv[0]);
                    return -1;
                }
            }
            LOG_INFO("Start sampling: type=%d", sample_type);
            commands.push_back({CMD_START_SAMPLE, 0, 0, 0, 0, 0, 0, sample_type, 0, 0, 0.0f});
            hasCommand = true;
        } else if (strcmp(argv[i], "--stop-sampling") == 0 && i + 1 < argc) {
            int sample_type;
            if (isdigit(argv[i+1][0])) {
                sample_type = std::stoi(argv[++i]);
            } else {
                sample_type = sampleTypeToValue(argv[++i]);
                if (sample_type == -1) {
                    LOG_ERROR("Invalid sample type: %s", argv[i]);
                    printHelp(argv[0]);
                    return -1;
                }
            }
            LOG_INFO("Stop sampling: type=%d", sample_type);
            commands.push_back({CMD_STOP_SAMPLE, 0, 0, 0, 0, 0, 0, sample_type, 0, 0, 0.0f});
            hasCommand = true;
        } else if (strcmp(argv[i], "--volt") == 0 && i + 2 < argc) {
            int id = std::stoi(argv[++i]);
            float target = std::stof(argv[++i]);
            if (id < 0 || id >= 18) {
                LOG_ERROR("Power supply ID out of range: %d (0-17)", id);
                printHelp(argv[0]);
                return -1;
            }
            LOG_INFO("Voltage set request: ID=%d, target=%.3f V", id, target);
            commands.push_back({CMD_VOLT_SET, 0, 0, 0, 0, 0, 0, 0, 0, id, target});
            hasCommand = true;
        } else if (strcmp(argv[i], "--all-off") == 0) {
            LOG_INFO("Turning off all power supplies (DAC=0, disable pins)");
            for (int id = 0; id < 18; ++id) {
                const auto& cfg = g_power_supplies[id];
                commands.push_back({CMD_DAC, cfg.dac_device, cfg.dac_channel, 0, 0, 0, 0, 0, 0, 0, 0.0f});
                commands.push_back({CMD_PIN, 0, 0, 0, cfg.enable_port, cfg.enable_pin, 0, 0, 0, 0, 0.0f});
            }
            hasCommand = true;
        } else if (strcmp(argv[i], "--cal-on") == 0) {
            LOG_INFO("Calibration mode enabled");
            commands.push_back({CMD_CAL_ON, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0f});
            hasCommand = true;
        } else if (strcmp(argv[i], "--cal-off") == 0) {
            LOG_INFO("Calibration mode disabled");
            commands.push_back({CMD_CAL_OFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0f});
            hasCommand = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printHelp(argv[0]);
            return 0;
        } else {
            LOG_ERROR("Unknown option: %s", argv[i]);
            printHelp(argv[0]);
            return -1;
        }
    }

    if (!hasCommand) {
        LOG_WARN("No command provided. Use --help for usage.");
        return 0;
    }

    USBDriver dev(STM32_VID, pid);
    if (!dev.open()) {
        LOG_ERROR("Cannot open device. Check VID/PID and connection.");
        LOG_ERROR("Press Enter to exit...");
        std::cin.get();
        return -1;
    }

    dev.setReceiveCallback(onDataReceived);

    // 执行命令
    for (const auto& cmd : commands) {
        switch (cmd.type) {
            case CMD_DAC:
                SendDACConfig(dev, cmd.dev, cmd.ch, cmd.dac);
                break;
            case CMD_PIN:
                SendPinConfig(dev, cmd.port, cmd.pin, cmd.level);
                break;
            case CMD_START_SAMPLE: {
                uint8_t start[] = {0x03, 0x01, static_cast<uint8_t>(cmd.sample_type)};
                dev.send(start, sizeof(start));
                LOG_INFO("Sampling started (type=%d)", cmd.sample_type);
                break;
            }
            case CMD_STOP_SAMPLE: {
                uint8_t stop[] = {0x03, 0x00, static_cast<uint8_t>(cmd.sample_type)};
                dev.send(stop, sizeof(stop));
                LOG_INFO("Sampling stopped (type=%d)", cmd.sample_type);
                break;
            }
            case CMD_SAMPLE: {
                uint8_t start[] = {0x03, 0x01, static_cast<uint8_t>(cmd.sample_type)};
                dev.send(start, sizeof(start));
                LOG_INFO("Sampling started for %d seconds...", cmd.duration);
                std::this_thread::sleep_for(std::chrono::seconds(cmd.duration));
                uint8_t stop[] = {0x03, 0x00, static_cast<uint8_t>(cmd.sample_type)};
                dev.send(stop, sizeof(stop));
                LOG_INFO("Sampling stopped.");
                break;
            }
            case CMD_VOLT_SET: {
                if (g_calibration_enabled) {
                    SetVoltageWithCalibration(dev, cmd.power_id, cmd.target_voltage);
                } else {
                    const auto& cfg = g_power_supplies[cmd.power_id];
                    int dac = CalculateDACValue(cmd.power_id, cmd.target_voltage);
                    int mv = static_cast<int>(dac * cfg.Vmax * 1000.0f / 4096.0f);
                    LOG_INFO("Setting power ID %d to %.3f V (DAC=%d, mv=%d)", cmd.power_id, cmd.target_voltage, dac, mv);
                    SendDACConfig(dev, cfg.dac_device, cfg.dac_channel, dac);
                    SendPinConfig(dev, cfg.enable_port, cfg.enable_pin, 1);       
                    // 打印电压信息（可选）
                    LogVoltageInfo(dev, cmd.power_id, cmd.target_voltage);
                }
                break;
            }
            case CMD_CAL_ON:
                g_calibration_enabled = true;
                LOG_INFO("Calibration mode enabled");
                break;
            case CMD_CAL_OFF:
                g_calibration_enabled = false;
                LOG_INFO("Calibration mode disabled");
                break;
        }
    }

    std::cin.get();
    dev.close();
    LOG_INFO("All commands executed. Press Enter to exit...");
    return 0;
}