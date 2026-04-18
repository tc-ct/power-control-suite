#include <fstream>
#include <string>
#include <cstring>
#include <array>

#include "file_parse.h"
#include "json.hpp"

static bool parse_power_up_sequence(const nlohmann::json& seq_array, SequenceConfig out_seq[POWER_SUPPLY_COUNT]) {
    if (!seq_array.is_array() || seq_array.size() != POWER_SUPPLY_COUNT) {
        return false;
    }

    std::array<bool, POWER_SUPPLY_COUNT> used = {false};

    for (size_t i = 0; i < seq_array.size(); ++i) {
        const auto& item = seq_array[i];
        if (!item.contains("id") || !item.contains("delay_ms")) {
            return false;
        }

        int id = item["id"];
        int delay_ms = item["delay_ms"];
        if (id < 0 || id >= POWER_SUPPLY_COUNT || delay_ms < 0 || used[id]) {
            return false;
        }

        out_seq[i].sequence = id;
        out_seq[i].interval_ms = delay_ms;

        if (item.contains("n") && item["n"].is_string()) {
            std::string name = item["n"];
            strncpy(out_seq[i].name, name.c_str(), sizeof(out_seq[i].name) - 1);
            out_seq[i].name[sizeof(out_seq[i].name) - 1] = '\0';
        } else {
            out_seq[i].name[0] = '\0';
        }

        used[id] = true;
    }

    for (int id = 0; id < POWER_SUPPLY_COUNT; ++id) {
        if (!used[id]) {
            return false;
        }
    }

    return true;
}

static bool parse_power_supply_object(const nlohmann::json& item, PowerSupplyConfig* cfg, int* out_power_id) {
    if (!cfg || !out_power_id) {
        return false;
    }

    memset(cfg, 0, sizeof(*cfg));

    try {
        // 验证必需字段存在
        if (!item.contains("id") || !item.contains("r1") || !item.contains("r2") ||
            !item.contains("r3") || !item.contains("dac_dev") || !item.contains("dac_ch") ||
            !item.contains("vfb") || !item.contains("vmax") || !item.contains("en_port") ||
            !item.contains("en_pin") || !item.contains("means_pt") || !item.contains("tgt_v")) {
            return false;
        }

        cfg->power_id = item["id"];
        if (cfg->power_id < 0 || cfg->power_id >= POWER_SUPPLY_COUNT) {
            return false;
        }
        *out_power_id = cfg->power_id;

        // 可选字段：name
        if (item.contains("n") && item["n"].is_string()) {
            std::string name = item["n"];
            strncpy(cfg->name, name.c_str(), sizeof(cfg->name) - 1);
            cfg->name[sizeof(cfg->name) - 1] = '\0';
        } else {
            cfg->name[0] = '\0';
        }

        cfg->R1 = item["r1"];
        cfg->R2 = item["r2"];
        cfg->R3 = item["r3"];
        cfg->dac_device = item["dac_dev"];
        cfg->dac_channel = item["dac_ch"];
        cfg->Vfb = item["vfb"];
        cfg->Vmax = item["vmax"];
        cfg->enable_port = item["en_port"];
        cfg->enable_pin = item["en_pin"];
        cfg->means_pt = item["means_pt"];
        cfg->tgt_volt = item["tgt_v"];

        if (cfg->means_pt < 0 || cfg->means_pt >= 27) {
            return false;
        }

        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

static bool parse_power_supplies(const nlohmann::json& supplies, PowerSupplyConfig out_supplies[POWER_SUPPLY_COUNT]) {
    if (!supplies.is_array() || supplies.size() != POWER_SUPPLY_COUNT) {
        return false;
    }

    std::array<PowerSupplyConfig, POWER_SUPPLY_COUNT> temp_configs;
    std::array<bool, POWER_SUPPLY_COUNT> loaded = {false};
    int loaded_count = 0;

    for (const auto& item : supplies) {
        int power_id = -1;
        PowerSupplyConfig cfg;
        if (!parse_power_supply_object(item, &cfg, &power_id)) {
            return false;
        }

        if (loaded[power_id]) {
            return false;
        }

        temp_configs[power_id] = cfg;
        loaded[power_id] = true;
        loaded_count++;
    }

    if (loaded_count != POWER_SUPPLY_COUNT) {
        return false;
    }

    memcpy(out_supplies, temp_configs.data(), sizeof(temp_configs));
    return true;
}

static bool parse_power_sample_object(const nlohmann::json& item, SampleConfig* cfg, int* out_sample_id) {
    if (!cfg || !out_sample_id) {
        return false;
    }

    memset(cfg, 0, sizeof(*cfg));

    try {
        if (!item.contains("id") || !item.contains("volt_en") || !item.contains("curr_en")) {
            return false;
        }

        const int sample_id = item["id"];
        if (sample_id < 0 || sample_id >= SAMPLE_DATA_COUNT) {
            return false;
        }

        *out_sample_id = sample_id;
        cfg->volt_en = item["volt_en"];
        cfg->current_en = item["curr_en"];

        if (item.contains("n") && item["n"].is_string()) {
            const std::string name = item["n"];
            strncpy(cfg->name, name.c_str(), sizeof(cfg->name) - 1);
            cfg->name[sizeof(cfg->name) - 1] = '\0';
        } else {
            cfg->name[0] = '\0';
        }

        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

static bool parse_power_samples(const nlohmann::json& samples, SampleConfig out_samples[SAMPLE_DATA_COUNT]) {
    if (!samples.is_array() || samples.size() != SAMPLE_DATA_COUNT) {
        return false;
    }

    std::array<SampleConfig, SAMPLE_DATA_COUNT> temp_configs{};
    std::array<bool, SAMPLE_DATA_COUNT> loaded = {false};
    int loaded_count = 0;

    for (const auto& item : samples) {
        int sample_id = -1;
        SampleConfig cfg;
        if (!parse_power_sample_object(item, &cfg, &sample_id)) {
            return false;
        }

        if (loaded[sample_id]) {
            return false;
        }

        temp_configs[sample_id] = cfg;
        loaded[sample_id] = true;
        loaded_count++;
    }

    if (loaded_count != SAMPLE_DATA_COUNT) {
        return false;
    }

    memcpy(out_samples, temp_configs.data(), sizeof(temp_configs));
    return true;
}

bool LoadPowerConfig(const char* file_path, PowersConfig* out_configs) {
    if (!file_path || !out_configs) {
        return false;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    try {
        // 读取文件内容
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // 解析JSON
        nlohmann::json j = nlohmann::json::parse(content);

        // 验证根结构
        if (!j.contains("power_supplies") || !j["power_supplies"].is_array()) {
            return false;
        }

        if (j.contains("power_calibration_en") && j["power_calibration_en"].is_boolean()) {
            out_configs->calibration_en = j["power_calibration_en"];
        }
        if (j.contains("power_volt_sample_en") && j["power_volt_sample_en"].is_boolean()) {
            out_configs->volt_sample_en = j["power_volt_sample_en"];
        }
        if (j.contains("power_curr_sample_en") && j["power_curr_sample_en"].is_boolean()) {
            out_configs->curr_sample_en = j["power_curr_sample_en"];
        }

        if (j.contains("power_up_sequence")) {
            if (!parse_power_up_sequence(j["power_up_sequence"], out_configs->sequences)) {
                return false;
            }
        }

        if (j.contains("power_supplies")) {
            if (!parse_power_supplies(j["power_supplies"], out_configs->supplies)) {
                return false;
            }
        }

        if (j.contains("power_samples")) {
            if (!parse_power_samples(j["power_samples"], out_configs->sample_cfg)) {
                return false;
            }
        }

        return true;

    } catch (const nlohmann::json::parse_error&) {
        return false;
    } catch (const nlohmann::json::exception&) {
        return false;
    } catch (const std::exception&) {
        return false;
    }
}
