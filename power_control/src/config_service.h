#ifndef CONFIG_SERVICE_H
#define CONFIG_SERVICE_H

#include <QString>

#include "power_config.h"

class ConfigService
{
public:
    bool load(const QString& filePath, QString* errorMessage = nullptr);

    const PowersConfig& config() const;
    PowersConfig& mutableConfig();

private:
    PowersConfig config_{};
};

#endif // CONFIG_SERVICE_H
