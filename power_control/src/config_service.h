#ifndef CONFIG_SERVICE_H
#define CONFIG_SERVICE_H

#include <QString>
#include <array>

#include "power_control.h"
#include "power_config.h"

class ConfigService
{
public:
	bool load(const QString& filePath, QString* errorMessage = nullptr);
	bool saveCalibrationResults(const QString& filePath,
	                            const PowerController::VoltageCalibrationResults& results,
	                            QString* errorMessage = nullptr) const;
	bool loadCalibrationTargets(const QString& filePath,
	                            std::array<float, POWER_SUPPLY_COUNT>& targets,
	                            std::array<bool, POWER_SUPPLY_COUNT>& loaded,
	                            QString* errorMessage = nullptr) const;

	const PowersConfig &config() const;
	PowersConfig &mutableConfig();

private:
	PowersConfig config_{};
};

#endif // CONFIG_SERVICE_H
