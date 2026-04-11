#include "waveform_recorder.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

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
    stream << "voltage_timestamp_ms,voltage_type";
    for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
        stream << ",channel_" << i;
    }
    stream << ",current_timestamp_ms,current_type";
    for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
        stream << ",channel_" << i;
    }
    stream << "\n";

    const int kBlockColumns = 2 + SAMPLE_DATA_COUNT;
    auto writeEmptyColumns = [&](int columnCount) {
        for (int i = 0; i < columnCount; ++i) {
            stream << ',';
        }
    };

    for (const SampleDataPacket& packet : packets_) {
        if (packet.type == I2C_DATA_VBUS) {
            stream << packet.timestamp << ",voltage";
            for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
                stream << ',' << packet.channel_volt_mv[i];
            }
            writeEmptyColumns(kBlockColumns);
            stream << "\n";
            continue;
        }

        if (packet.type == I2C_DATA_CURRENT) {
            writeEmptyColumns(kBlockColumns);
            stream << packet.timestamp << ",current";
            for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
                stream << ',' << packet.channel_curr_ma[i];
            }
            stream << "\n";
            continue;
        }

        writeEmptyColumns(kBlockColumns * 2);
        stream << "\n";
    }

    last_record_directory_ = QFileInfo(filePath).absolutePath();
    return true;
}
