#include <thread>
#include <chrono>
#include <cmath>

#include "log.h"
#include "stm32_comm.h"
#include "power_control.h"

// Initialize controller with runtime power configuration.
PowerController::PowerController(PowersConfig* configs)
    : configs_(configs) {}

// Validate power rail id range.
bool PowerController::IsValidPowerId(int id) const {
    return id >= 0 && id < POWER_SUPPLY_COUNT;
}

// Convert target voltage to DAC code based on resistor network model.
int PowerController::CalculateDACValue(int id, float target_voltage) const {
    if (!IsValidPowerId(id)) {
        LOG_ERROR("Invalid power supply ID: %d", id);
        return 0;
    }
    const auto& cfg = configs_->supplies[id];
    float Vfb = cfg.Vfb;
    float R1 = cfg.R1;
    float R2 = cfg.R2;
    float R3 = cfg.R3;
    float Vmax = cfg.Vmax;

    float term1 = Vfb;
    float term2 = target_voltage - Vfb * (1 + R1 / R2);
    float term3 = term2 * R3 / R1;
    float Vdac = term1 - term3;

    if (Vdac < 0.0f) Vdac = 0.0f;
    if (Vdac > Vmax) Vdac = Vmax;

    return static_cast<int>(Vdac / Vmax * static_cast<float>(kDacMaxCode));
}

// Sample voltage multiple times and return the average value.
float PowerController::GetAverageVoltage(USBDriver& dev, int means_pt, int samples) const {
    if (means_pt < 0 || means_pt >= 27) {
        LOG_ERROR("Invalid means_pt %d", means_pt);
        return -1.0f;
    }
    if (samples <= 0) {
        LOG_ERROR("Invalid samples count %d", samples);
        return -1.0f;
    }

    float sum = 0.0f;
    int valid_count = 0;
    for (int i = 0; i < samples; ++i) {
        float volt = GetActualVoltage(dev, means_pt);
        if (volt >= 0) {
            sum += volt;
            ++valid_count;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (valid_count == 0) {
        LOG_ERROR("No valid voltage readings for means_pt %d", means_pt);
        return -1.0f;
    }
    return sum / valid_count;
}

// Read and log voltage deviation for a given power rail.
void PowerController::LogVoltageInfo(USBDriver& dev, int power_id, float target_voltage) const {
    if (!IsValidPowerId(power_id)) {
        LOG_ERROR("Invalid power ID %d", power_id);
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    float actual_voltage = GetActualVoltage(dev, power_id);
    if (actual_voltage < 0) {
        LOG_ERROR("Failed to read actual voltage for power ID %d", power_id);
        return;
    }

    float error = actual_voltage - target_voltage;
    LOG_INFO("Power ID %d: target=%.3f V, actual=%.3f V, error=%.3f V",
             power_id, target_voltage, actual_voltage, error);
}

// Enable one power rail through its GPIO enable pin.
void PowerController::EnablePower(USBDriver& dev, int power_id) const {
    if (!IsValidPowerId(power_id)) {
        LOG_ERROR("Invalid power ID %d", power_id);
        return;
    }
    const auto& cfg = configs_->supplies[power_id];
    SendPinConfig(dev, cfg.enable_port, cfg.enable_pin, true);


}

// Program one rail DAC output to the requested target voltage.
void PowerController::ConfigVoltage(USBDriver& dev, int power_id, float target_voltage) const {
    auto& cfg = configs_->supplies[power_id];
    cfg.dac_value = CalculateDACValue(power_id, target_voltage);
    SendVoltageConfig(dev, &cfg);
    int mv = static_cast<int>(cfg.dac_value * cfg.Vmax * 1000.0f / static_cast<float>(kDacMaxCode));
    LOG_INFO("Setting power ID %d to %.3f V (DAC=%d, mv=%d)", power_id, target_voltage, cfg.dac_value, mv);
}


// Apply all configured voltages, then enable rails, and run optional calibration.
void PowerController::ConfigVoltages(USBDriver& dev) const {

    // enable rails by configured power-up sequence
    SendPowerOn(dev, configs_);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // config all voltages first
    for (int power_id = 0; power_id < POWER_SUPPLY_COUNT; ++power_id) {
        const auto& cfg = configs_->supplies[power_id];
        ConfigVoltage(dev, power_id, cfg.tgt_volt);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if(configs_->calibration_en) {
        LOG_INFO("Calibration enabled, starting calibration process...");
        LOG_INFO("==========================================");
        LOG_INFO("Starting voltage calibration for all rails");
        LOG_INFO("==========================================");
        for (int power_id = 0; power_id < POWER_SUPPLY_COUNT; ++power_id) {
            const auto& cfg = configs_->supplies[power_id];
            CalibrateVoltage(dev, power_id);
        }
        LOG_INFO("==========================================");
        LOG_INFO("Calibration process completed");
        LOG_INFO("==========================================");

        // Display final calibration results for all power rails
        LOG_INFO("Final Calibration Results:");
        LOG_INFO("==========================================");
        for (int power_id = 0; power_id < POWER_SUPPLY_COUNT; ++power_id) {
            const auto& cfg = configs_->supplies[power_id];
            float target_voltage = cfg.tgt_volt;
            float actual_voltage = GetAverageVoltage(dev, cfg.means_pt, 5);  // Use fewer samples for summary

            if (actual_voltage < 0) {
                LOG_ERROR("Power ID %d (%s): Failed to read voltage", power_id, cfg.name);
                continue;
            }

            float error = actual_voltage - target_voltage;
            float tolerance = target_voltage * 0.007f + 0.003125f;
            bool success = std::abs(error) <= tolerance;

            LOG_INFO("Power ID %d (%s): Target=%.3f V, Actual=%.3f V, Error=%.3f V [%s]",
                     power_id, cfg.name, target_voltage, actual_voltage, error,
                     success ? "SUCCESS" : "FAILED");
        }
        LOG_INFO("==========================================");
    }
    else {
       SendPowerOn(dev, configs_);
    }

}


// Iteratively calibrate one rail to reduce voltage error.
void PowerController::CalibrateVoltage(USBDriver& dev, int power_id) const {
    if (!IsValidPowerId(power_id)) {
        LOG_ERROR("Invalid power ID %d", power_id);
        return;
    }

    const auto& cfg = configs_->supplies[power_id];
    float target_voltage = cfg.tgt_volt;
    float tolerance = target_voltage * 0.007f + 0.003125f;
    bool satisfied = false;
    float actual_voltage = 0.0f;

    float current_target = target_voltage;
    for (int iter = 0; iter < kMaxCalibrationIterations; ++iter) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        actual_voltage = GetAverageVoltage(dev, cfg.means_pt, kSampleCount);
        if (actual_voltage < 0) {
            LOG_ERROR("Failed to read voltage, abort calibration");
            break;
        }

        float error = actual_voltage - target_voltage;
        float abs_error = std::abs(error);
 /*       if (abs_error > 2.5f) {
            LOG_WARN("Voltage error too large (%.4f V), aborting calibration for power ID %d", abs_error, power_id);
            break;
        }
*/
        satisfied = (abs_error <= tolerance);

        float compensated_target = current_target + (target_voltage - actual_voltage);
        if (compensated_target < 0) compensated_target = 0;
        current_target = compensated_target;

        LOG_INFO("Iteration %d: actual=%.4f V, target=%.4f V, error=%.4f V, tolerance=%.4f V, new_target=%.4f V",
                 iter + 1, actual_voltage, target_voltage, error, tolerance, current_target);

        ConfigVoltage(dev, power_id, current_target);
    }

    if (satisfied) {
        LOG_INFO("Calibration successful: Power: %d , actual=%.4f, Vtarget=%.4f V", power_id, actual_voltage, target_voltage);
    } else {
        LOG_WARN("Calibration did not meet tolerance after %d iterations: target=%.4f V, actual=%.4f V",
                 kMaxCalibrationIterations, target_voltage, actual_voltage);
    }
}