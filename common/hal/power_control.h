#ifndef POWER_H
#define POWER_H

#include "usb_driver.h"
#include "power_config.h"

class PowerController {
public:
	PowerController(PowersConfig* configs);
    void ConfigVoltages(USBDriver& dev) const;
    void EnablePower(USBDriver& dev, int power_id) const;

private:
	int CalculateDACValue(int id, float target_voltage) const;
    float GetAverageVoltage(USBDriver& dev, int power_id, int samples) const;
	bool IsValidPowerId(int id) const;
    void CalibrateVoltage(USBDriver& dev, int power_id) const;
    void ConfigVoltage(USBDriver& dev, int power_id, float target_voltage) const;

	PowersConfig* configs_;

	static const int kDacMaxCode = 4095;
    static const int kSampleCount = 6;
	static const int kMaxCalibrationIterations = 3;
};

#endif // POWER_H