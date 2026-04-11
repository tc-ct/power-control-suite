#include "sampling_state_machine.h"

SamplingStateMachine::State SamplingStateMachine::state() const {
    return state_;
}

bool SamplingStateMachine::isOpen() const {
    return state_ != State::Disconnected;
}

bool SamplingStateMachine::isSampling() const {
    return state_ == State::Sampling;
}

void SamplingStateMachine::onDeviceOpened() {
    state_ = State::ConnectedIdle;
}

void SamplingStateMachine::onDeviceClosed() {
    state_ = State::Disconnected;
}

void SamplingStateMachine::onSamplingStarted() {
    if (state_ == State::ConnectedIdle) {
        state_ = State::Sampling;
    }
}

void SamplingStateMachine::onSamplingStopped() {
    if (state_ == State::Sampling) {
        state_ = State::ConnectedIdle;
    }
}
