#ifndef POWER_H
#define POWER_H

#include <array>

#include "usb_driver.h"
#include "power_config.h"

struct VoltageCalibrationResult {
	int power_id = -1;
	char name[POWER_SUPPLY_NAME_MAX] = {};
	float target_voltage = 0.0f;
	float calibrated_target_voltage = 0.0f;
	float final_actual_voltage = -1.0f;
	bool success = false;
};

class PowerController
{
public:
	using VoltageCalibrationResults = std::array<VoltageCalibrationResult, POWER_SUPPLY_COUNT>;

	PowerController(PowersConfig* configs);
	void ConfigVoltages(USBDriver& dev) const;
	void ConfigVoltage(USBDriver& dev, int power_id, float target_voltage) const;
	void EnablePower(USBDriver& dev, int power_id) const;
	VoltageCalibrationResults CalibrateVoltages(USBDriver& dev) const;

private:
	int CalculateDACValue(int id, float target_voltage) const;
	float GetAverageVoltage(USBDriver& dev, int power_id, int samples) const;
	bool IsValidPowerId(int id) const;
	VoltageCalibrationResult CalibrateVoltage(USBDriver& dev, int power_id) const;

	PowersConfig *configs_;

	static const int kDacMaxCode = 4095;
	static const int kSampleCount = 6;
	static const int kMaxCalibrationIterations = 5;
};

#endif // POWER_H
