#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <array>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "power_config.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

class QTableWidgetItem;
class WaveformWidget;
class DebugInterfaceWindow;
class DeviceSessionService;
class ConfigService;
class QAction;
class QPushButton;
class WaveformRecorder;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

private slots:
    void onQueryDevices();
    void onToggleOpenDevice();
    void onToggleSampling();
    void onPowerOnClicked();
    void onToggleCalibrationClicked();
    void onBrowseConfig();
    void onLoadConfig();
    void onSampleTableItemChanged(QTableWidgetItem* item);
    void onOpenDebugInterface();
    void onToggleTestMode(bool enabled);
    void onToggleRecording();

private:
    QString currentConfigPath() const;
    QString defaultConfigPath() const;
    void addConfigPathOption(const QString& filePath);
    void refreshSampleTable();
    void onDataReceived(const SampleDataPacket& packet);
    void updateSampleValuesFromPacket(const SampleDataPacketTF& packet);
    void appendLog(const QString& message);
    void refreshDeviceList();
    void updateConnectionState();
    QString selectedDevicePath() const;
    void startTestModeThread();
    void stopTestModeThread();
    void stopRecording(bool exportFile);
    void refreshCalibrationButtonText();
    void waitPowerOnThread();

    Ui::MainWindow *ui;
    std::unique_ptr<DeviceSessionService> session_service_;
    std::unique_ptr<ConfigService> config_service_;
    std::array<float, SAMPLE_DATA_COUNT> sampled_volt_{};
    std::array<float, SAMPLE_DATA_COUNT> sampled_curr_{};
    std::array<bool, SAMPLE_DATA_COUNT> has_sampled_volt_{};
    std::array<bool, SAMPLE_DATA_COUNT> has_sampled_curr_{};
    bool refreshing_sample_table_ = false;
    WaveformWidget* waveform_widget_ = nullptr;
    std::vector<DebugInterfaceWindow*> debug_windows_;
    QAction* test_mode_action_ = nullptr;
    QPushButton* power_on_button_ = nullptr;
    QPushButton* calibration_toggle_button_ = nullptr;
    QPushButton* record_button_ = nullptr;
    std::unique_ptr<WaveformRecorder> waveform_recorder_;
    std::atomic<bool> power_on_running_{false};
    std::thread power_on_thread_;
    std::atomic<bool> test_mode_running_{false};
    std::thread test_mode_thread_;
};
#endif // MAINWINDOW_H
