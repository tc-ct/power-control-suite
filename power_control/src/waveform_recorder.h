#ifndef WAVEFORM_RECORDER_H
#define WAVEFORM_RECORDER_H

#include <QString>

#include <vector>

#include "power_config.h"

class WaveformRecorder
{
public:
	void start(const QString& baseDirectory);
	void stop();
	bool isRecording() const;

	void appendPacket(const SampleDataPacket& packet);
	bool hasData() const;
	void clear();

	QString defaultFilePath() const;
	QString lastRecordDirectory() const;
	bool exportCsv(const QString& filePath);

private:
	bool is_recording_ = false;
	std::vector<SampleDataPacket> packets_;
	QString last_record_directory_;
};

#endif // WAVEFORM_RECORDER_H
