#ifndef DEVICE_SESSION_SERVICE_H
#define DEVICE_SESSION_SERVICE_H

#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <vector>

#include "power_config.h"
#include "sampling_state_machine.h"
#include "proto_pkg.h"
#include "usb_driver.h"

class DeviceSessionService : public QObject
{
	Q_OBJECT

public:
	explicit DeviceSessionService(uint16_t vid, uint16_t pid, QObject* parent = nullptr);
	~DeviceSessionService() override;

	void queryDevices();
	const std::vector<USBDeviceInfo> &devices() const;

	bool openDevice(const QString& path);
	void closeDevice();

	bool isOpen() const;
	bool isSampling() const;

	void startSampling(const PowersConfig& config);
	void stopSampling(const PowersConfig& config);
	bool sendDebugRequest(const DebugRequestPacket_t& request);
	uint16_t allocateDebugRequestId();
	bool enterDebugSession();
	void exitDebugSession();

	USBDriver *driver() const;

signals:
	void devicesChanged();
	void connectionChanged(bool isOpen);
	void samplingChanged(bool sampling);
	void samplePacketReceived(const SampleDataPacket& packet);
	void debugResponseReceived(const DebugResponsePacket_t& packet);

private:
	void sendSamplingCommand(bool start, const PowersConfig& config);
	void onRawDataReceived(const uint8_t* data, int length);

	uint16_t vid_;
	uint16_t pid_;
	std::vector<USBDeviceInfo> devices_;
	std::unique_ptr<USBDriver> usb_driver_;
	SamplingStateMachine state_machine_;
	PowersConfig last_sampling_config_{};
	bool has_last_sampling_config_ = false;
	uint16_t next_debug_req_id_ = 1;
	int debug_session_depth_ = 0;
	bool debug_resume_sampling_ = false;
};

#endif // DEVICE_SESSION_SERVICE_H
