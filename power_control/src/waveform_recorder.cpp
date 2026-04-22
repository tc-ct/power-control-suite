#include "waveform_recorder.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include "stm32_comm.h"

void WaveformRecorder::start(const QString& baseDirectory)
{
    packets_.clear();
    last_record_directory_ = baseDirectory;
    is_recording_ = true;
}

void WaveformRecorder::stop()
{
    is_recording_ = false;
}

bool WaveformRecorder::isRecording() const
{
    return is_recording_;
}

void WaveformRecorder::appendPacket(const SampleDataPacket& packet)
{
    if (!is_recording_) {
        return;
    }
    packets_.push_back(packet);
}

bool WaveformRecorder::hasData() const
{
    return !packets_.empty();
}

void WaveformRecorder::clear()
{
    packets_.clear();
}

QString WaveformRecorder::defaultFilePath() const
{
    const QString baseDirectory = last_record_directory_.isEmpty()
        ? QDir::currentPath()
        : last_record_directory_;
    const QString fileName = QStringLiteral("waveform_record_%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    return QDir(baseDirectory).filePath(fileName);
}

QString WaveformRecorder::lastRecordDirectory() const
{
    return last_record_directory_;
}

bool WaveformRecorder::exportCsv(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream stream(&file);
    stream << "voltage_timestamp_ms";
    for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
        stream << ",channel_" << i << "_mv";
    }
    stream << ",current_timestamp_ms";
    for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
        stream << ",channel_" << i << "_ma";
    }
    stream << "\n";

    for (const SampleDataPacket& packet : packets_) {
        SampleDataPacketTF_t parsed{};
        Protocol_ParseSampleData(
            reinterpret_cast<const uint8_t*>(&packet),
            static_cast<int>(sizeof(packet)),
            &parsed);

        stream << packet.timestamp;
        for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
            stream << ',' << parsed.channel_volt_mv[i];
        }
        stream << ',' << packet.timestamp;
        for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
            stream << ',' << parsed.channel_curr_ma[i];
        }
        stream << "\n";
    }

    last_record_directory_ = QFileInfo(filePath).absolutePath();
    return true;
}
