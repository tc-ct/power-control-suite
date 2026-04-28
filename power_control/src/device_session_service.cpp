#include "device_session_service.h"

#include <QMetaObject>

#include <cstring>

#include "stm32_comm.h"

namespace {
constexpr uint8_t kAdcReportId = 0x07;
}

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
    sample_packet_gate_state_.store(SamplePacketGateState::PassThrough);
    emit samplingChanged(false);
    emit connectionChanged(true);
    return true;
}

void DeviceSessionService::closeDevice() {
    if (!usb_driver_) {
        state_machine_.onDeviceClosed();
        debug_session_depth_ = 0;
        debug_resume_sampling_ = false;
        sample_packet_gate_state_.store(SamplePacketGateState::PassThrough);
        emit samplingChanged(false);
        emit connectionChanged(false);
        return;
    }

    usb_driver_->close();
    usb_driver_.reset();
    state_machine_.onDeviceClosed();
    debug_session_depth_ = 0;
    debug_resume_sampling_ = false;
    sample_packet_gate_state_.store(SamplePacketGateState::PassThrough);
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

    last_sampling_config_ = config;
    has_last_sampling_config_ = true;
    sample_packet_gate_state_.store(SamplePacketGateState::DropFirstPacketAfterStart);
    sendSamplingCommand(true, config);
    state_machine_.onSamplingStarted();
    emit samplingChanged(true);
}

void DeviceSessionService::stopSampling(const PowersConfig& config) {
    if (!isOpen() || !isSampling()) {
        return;
    }

    last_sampling_config_ = config;
    has_last_sampling_config_ = true;
    sample_packet_gate_state_.store(SamplePacketGateState::DropUntilNextStart);
    sendSamplingCommand(false, config);
    state_machine_.onSamplingStopped();
    emit samplingChanged(false);
}

bool DeviceSessionService::sendDebugRequest(const DebugRequestPacket_t& request) {
    if (!usb_driver_ || !usb_driver_->isOpen()) {
        return false;
    }
    return usb_driver_->send(request.bytes, sizeof(request.bytes));
}

uint16_t DeviceSessionService::allocateDebugRequestId() {
    if (next_debug_req_id_ == 0) {
        next_debug_req_id_ = 1;
    }
    return next_debug_req_id_++;
}

bool DeviceSessionService::enterDebugSession() {
    if (!isOpen()) {
        return false;
    }

    if (debug_session_depth_ == 0) {
        debug_resume_sampling_ = isSampling();
        if (debug_resume_sampling_) {
            if (!has_last_sampling_config_) {
                debug_resume_sampling_ = false;
            } else {
                stopSampling(last_sampling_config_);
            }
        }
    }

    ++debug_session_depth_;
    return true;
}

void DeviceSessionService::exitDebugSession() {
    if (debug_session_depth_ <= 0) {
        debug_session_depth_ = 0;
        return;
    }

    --debug_session_depth_;
    if (debug_session_depth_ == 0 && debug_resume_sampling_) {
        debug_resume_sampling_ = false;
        if (isOpen() && has_last_sampling_config_ && !isSampling()) {
            startSampling(last_sampling_config_);
        }
    }
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
    if (!data || length <= 0) {
        return;
    }

    if (length >= static_cast<int>(sizeof(DebugResponsePacket_t))) {
        const auto* debugPacket = reinterpret_cast<const DebugResponsePacket_t*>(data);
        if (debugPacket->report_id == kAdcReportId
            && debugPacket->magic[0] == static_cast<uint8_t>('D')
            && debugPacket->magic[1] == static_cast<uint8_t>('B')
            && debugPacket->magic[2] == static_cast<uint8_t>('G')
            && debugPacket->magic[3] == static_cast<uint8_t>('1')) {
            DebugResponsePacket_t packet{};
            memcpy(&packet, data, sizeof(DebugResponsePacket_t));
            QMetaObject::invokeMethod(this, [this, packet]() {
                emit debugResponseReceived(packet);
            }, Qt::QueuedConnection);
            return;
        }
    }

    if (length < static_cast<int>(sizeof(SampleDataPacket))) {
        return;
    }

    SampleDataPacket packet{};
    memcpy(&packet, data, sizeof(SampleDataPacket));

    const SamplePacketGateState gate = sample_packet_gate_state_.load();
    if (gate == SamplePacketGateState::DropUntilNextStart) {
        return;
    }
    if (gate == SamplePacketGateState::DropFirstPacketAfterStart) {
        sample_packet_gate_state_.store(SamplePacketGateState::PassThrough);
        return;
    }

    QMetaObject::invokeMethod(this, [this, packet]() {
        emit samplePacketReceived(packet);
    }, Qt::QueuedConnection);
}
