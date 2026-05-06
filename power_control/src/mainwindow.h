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
class QComboBox;

enum class SampleTableDisplayMode {
    PowerId,
    SampleChannel,
};

struct SampleTableRowMapping {
    int display_id = -1;
    int power_id = -1;
    int sample_id = -1;
    int data_index = -1;
    bool valid = false;
};

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
    bool sendDebugRequestAndWait(const DebugRequestPacket_t& request, DebugResponsePacket_t& response, int timeoutMs = 1000);
    bool sendDebugRequestAndWaitInSession(const DebugRequestPacket_t& request, DebugResponsePacket_t& response, int timeoutMs = 1000);
    void configureIna238ShuntCalibration(const PowersConfig& config);
    uint16_t calculateIna238ShuntCal(const SampleConfig& sampleCfg) const;
    QString currentConfigPath() const;
    QString defaultConfigPath() const;
    void addConfigPathOption(const QString& filePath);
    void setupSampleModeSelector();
    void rebuildSampleRowMap();
    int sampleIdForRow(int row) const;
    int dataIndexForRow(int row) const;
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
    SampleTableDisplayMode sample_table_mode_ = SampleTableDisplayMode::PowerId;
    std::vector<SampleTableRowMapping> sample_row_map_{};
    bool refreshing_sample_table_ = false;
    QComboBox* sample_mode_combo_ = nullptr;
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
