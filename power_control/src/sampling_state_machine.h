#ifndef SAMPLING_STATE_MACHINE_H
#define SAMPLING_STATE_MACHINE_H

class SamplingStateMachine
{
public:
	enum class State {
		Disconnected,
		ConnectedIdle,
		Sampling,
	};

	State state() const;
	bool isOpen() const;
	bool isSampling() const;

	void onDeviceOpened();
	void onDeviceClosed();
	void onSamplingStarted();
	void onSamplingStopped();

private:
	State state_ = State::Disconnected;
};

#endif // SAMPLING_STATE_MACHINE_H
