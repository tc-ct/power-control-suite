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
#include <cstring>
#include <string>
#include <getopt.h>
#include <cctype>
#include <Windows.h>

#include "log.h"
#include "usb_driver.h"
#include "file_parse.h"
#include "stm32_comm.h"
#include "power_control.h"
#include "proto_pkg.h"

// 设备 VID/PID
constexpr uint16_t STM32_VID = 0x0483;
constexpr uint16_t STM32_PID = 0x5750;  // 根据实际修改

// 接收数据回调函数
void onDataReceived(const uint8_t* data, int length) {
    if (length >= USB_REPORT_SIZE) {
        SampleDataPacketTF_t pk_tf;
        Protocol_ParseSampleData(data, length, &pk_tf);

    }else {
        // 原始数据打印
        printf("  Raw: ");
        for (int i = 0; i < length; ++i) {
            printf("%02X ", data[i]);
        }
        printf("\n");
    }
}

// 18路电源配置数组（由配置文件加载）
PowersConfig g_powers_config;

static void DumpPowersConfig(const PowersConfig& cfg) {
    LOG_INFO("===== Parsed Powers Config =====");
    LOG_INFO("calibration_en=%d", cfg.calibration_en ? 1 : 0);

    for (int i = 0; i < POWER_SUPPLY_COUNT; ++i) {
        const PowerSupplyConfig& s = cfg.supplies[i];
        LOG_INFO(
            "id=%d, name=%s, tgt_v=%.4f, means_pt=%d, dac_dev=%d, dac_ch=%d, en_port=%d, en_pin=%d, "
            "R1=%.4f, R2=%.4f, R3=%.4f, Vfb=%.4f, Vmax=%.4f",
            s.power_id,
            s.name,
            s.tgt_volt,
            s.means_pt,
            s.dac_device,
            s.dac_channel,
            s.enable_port,
            s.enable_pin,
            s.R1,
            s.R2,
            s.R3,
            s.Vfb,
            s.Vmax
        );
    }
    LOG_INFO("===== End Parsed Powers Config =====");
}

// 打印帮助信息
void printHelp(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --config <file>          Load power configuration from JSON file (default: power_config.json)\n");
    printf("  --help                   Show this help\n");
    printf("\nExamples:\n");
    printf("  %s --config ./config/power_config.json\n", prog);
}


int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    log_set_level(LOG_LEVEL_INFO);
    LOG_INFO("STM32 Custom HID Test Program");

    // 定义长选项
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    bool hasCommand = false;
    std::string config_path = "power_config.json";

    int opt;
    int option_index = 0;

    // 解析命令行参数
    while ((opt = getopt_long(argc, argv, "c:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':  // --config
                config_path = optarg;
                hasCommand = true;
                break;
            case 'h':  // --help
                printHelp(argv[0]);
                return 0;
            case '?':
                // getopt_long 会为未知选项打印错误消息
                printHelp(argv[0]);
                return -1;
            default:
                printHelp(argv[0]);
                return -1;
        }
    }

    if (!hasCommand) {
        LOG_WARN("No command provided. Use --help for usage.");
        return 0;
    }

    bool config_loaded = LoadPowerConfig(config_path.c_str(), &g_powers_config);
    if (config_loaded) {
        LOG_INFO("Loaded power configuration from %s", config_path.c_str());
        // DumpPowersConfig(g_powers_config);
    } else {
        LOG_ERROR("Loaded power configuration error.");
        return -1;
    }

    USBDriver dev(STM32_VID, STM32_PID);
    if (!dev.open()) {
        LOG_ERROR("Cannot open device. Check VID/PID and connection.");
        LOG_ERROR("Press Enter to exit...");
        std::cin.get();
        return -1;
    }

    dev.setReceiveCallback(onDataReceived);

    // CMD_VOLT_SET
    PowerController power(&g_powers_config);
    power.ConfigVoltages(dev);

    // Start sampling
    SendStartSample(dev, SAMPLE_TYPE_VOLTAGE);  // Assuming 4 corresponds to voltage type
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cin.get();
    SendStopSample(dev, SAMPLE_TYPE_VOLTAGE);

    dev.close();
    LOG_INFO("All commands executed. Press Enter to exit...");
    return 0;
}