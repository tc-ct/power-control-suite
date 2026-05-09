#include "config_service.h"

#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QSaveFile>

#include "file_parse.h"
#include "json.hpp"

bool ConfigService::load(const QString& filePath, QString* errorMessage) {
    PowersConfig loaded{};
    const QByteArray pathBytes = QDir::fromNativeSeparators(filePath).toLocal8Bit();
    if (!LoadPowerConfig(pathBytes.constData(), &loaded)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("power_config.json 解析失败，请检查文件格式。");
        }
        return false;
    }

    config_ = loaded;
    return true;
}

bool ConfigService::saveCalibrationResults(const QString& filePath,
                                           const PowerController::VoltageCalibrationResults& results,
                                           QString* errorMessage) const {
    nlohmann::json root;
    root["version"] = 1;
    root["calibration_results"] = nlohmann::json::array();
    for (const VoltageCalibrationResult& result : results) {
        root["calibration_results"].push_back({
            {"id", result.power_id},
            {"name", result.name},
            {"target_v", result.target_voltage},
            {"calibrated_tgt_v", result.calibrated_target_voltage},
            {"final_actual_v", result.final_actual_voltage},
            {"success", result.success},
        });
    }

    QSaveFile file(QDir::fromNativeSeparators(filePath));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法打开校准文件用于写入: %1").arg(filePath);
        }
        return false;
    }

    const std::string content = root.dump(4);
    file.write(content.data(), static_cast<qint64>(content.size()));
    file.write("\n", 1);
    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("保存校准文件失败: %1").arg(filePath);
        }
        return false;
    }

    return true;
}

bool ConfigService::loadCalibrationTargets(const QString& filePath,
                                           std::array<float, POWER_SUPPLY_COUNT>& targets,
                                           std::array<bool, POWER_SUPPLY_COUNT>& loaded,
                                           QString* errorMessage) const {
    targets.fill(0.0f);
    loaded.fill(false);

    QFile file(QDir::fromNativeSeparators(filePath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法打开校准文件: %1").arg(filePath);
        }
        return false;
    }

    try {
        const QByteArray content = file.readAll();
        const nlohmann::json root = nlohmann::json::parse(content.constData());
        if (!root.contains("calibration_results") || !root["calibration_results"].is_array()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("校准文件缺少 calibration_results 数组: %1").arg(filePath);
            }
            return false;
        }

        for (const auto& item : root["calibration_results"]) {
            if (!item.contains("id") || !item.contains("calibrated_tgt_v")) {
                continue;
            }
            const int id = item["id"];
            if (id < 0 || id >= POWER_SUPPLY_COUNT) {
                continue;
            }
            targets[id] = item["calibrated_tgt_v"];
            loaded[id] = true;
        }

        return true;
    } catch (const nlohmann::json::exception& ex) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("解析校准文件失败: %1").arg(QString::fromLocal8Bit(ex.what()));
        }
        return false;
    }
}

const PowersConfig& ConfigService::config() const {
    return config_;
}

PowersConfig& ConfigService::mutableConfig() {
    return config_;
}
