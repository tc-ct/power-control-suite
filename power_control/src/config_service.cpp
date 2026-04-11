#include "config_service.h"

#include <QDir>

#include "file_parse.h"

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

const PowersConfig& ConfigService::config() const {
    return config_;
}

PowersConfig& ConfigService::mutableConfig() {
    return config_;
}
