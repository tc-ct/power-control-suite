#include "device_session_service.h"

#include <QMetaObject>

#include <cstring>

#include "stm32_comm.h"

DeviceSessionService::DeviceSessionService(uint16_t vid, uint16_t pid, QObject* parent)
    : QObject(parent), vid_(vid), pid_(pid)
{
}

DeviceSessionService::~DeviceSessionService() {
    closeDevice();
}

void DeviceSessionService::queryDevices() {
    devices_ = USBDriver::queryDevices(vid_, pid_);
    emit devicesChanged();
}

const std::vector<USBDeviceInfo>& DeviceSessionService::devices() const {
    return devices_;
}

bool DeviceSessionService::openDevice(const QString& path) {
    if (path.isEmpty()) {
        return false;
    }

    closeDevice();
    usb_driver_ = std::make_unique<USBDriver>(vid_, pid_);

    if (!usb_driver_->open(path.toLocal8Bit().constData())) {
        usb_driver_.reset();
        emit connectionChanged(false);
        return false;
    }

    usb_driver_->setReceiveCallback([this](const uint8_t* data, int length) {
        onRawDataReceived(data, length);
    });

    state_machine_.onDeviceOpened();
    emit samplingChanged(false);
    emit connectionChanged(true);
    return true;
}

void DeviceSessionService::closeDevice() {
    if (!usb_driver_) {
        state_machine_.onDeviceClosed();
        emit samplingChanged(false);
        emit connectionChanged(false);
        return;
    }

    usb_driver_->close();
    usb_driver_.reset();
    state_machine_.onDeviceClosed();
    emit samplingChanged(false);
    emit connectionChanged(false);
}

bool DeviceSessionService::isOpen() const {
    return state_machine_.isOpen();
}

bool DeviceSessionService::isSampling() const {
    return state_machine_.isSampling();
}

void DeviceSessionService::startSampling(const PowersConfig& config) {
    if (!isOpen() || isSampling()) {
        return;
    }

    sendSamplingCommand(true, config);
    state_machine_.onSamplingStarted();
    emit samplingChanged(true);
}

void DeviceSessionService::stopSampling(const PowersConfig& config) {
    if (!isOpen() || !isSampling()) {
        return;
    }

    sendSamplingCommand(false, config);
    state_machine_.onSamplingStopped();
    emit samplingChanged(false);
}

USBDriver* DeviceSessionService::driver() const {
    return usb_driver_.get();
}

void DeviceSessionService::sendSamplingCommand(bool start, const PowersConfig& config) {
    if (!usb_driver_ || !usb_driver_->isOpen()) {
        return;
    }

    if (config.volt_sample_en) {
        if (start) {
            SendStartSample(*usb_driver_, SAMPLE_TYPE_VOLTAGE);
        } else {
            SendStopSample(*usb_driver_, SAMPLE_TYPE_VOLTAGE);
        }
    }

    if (config.curr_sample_en) {
        if (start) {
            SendStartSample(*usb_driver_, SAMPLE_TYPE_CURRENT);
        } else {
            SendStopSample(*usb_driver_, SAMPLE_TYPE_CURRENT);
        }
    }
}

void DeviceSessionService::onRawDataReceived(const uint8_t* data, int length) {
    if (!data || length < static_cast<int>(sizeof(SampleDataPacket))) {
        return;
    }

    SampleDataPacket packet{};
    memcpy(&packet, data, sizeof(SampleDataPacket));

    QMetaObject::invokeMethod(this, [this, packet]() {
        emit samplePacketReceived(packet);
    }, Qt::QueuedConnection);
}
