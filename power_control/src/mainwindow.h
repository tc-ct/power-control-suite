#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <array>
#include <atomic>
#include <cstdint>
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
class DeviceSessionService;
class ConfigService;
class QAction;
class QPushButton;
class WaveformRecorder;
class QComboBox;
class QLineEdit;
class QLabel;
class QCheckBox;

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
    void onStartCalibrationClicked();
    void onLoadCalibrationValues();
    void onRestoreDefaultValues();
    void onBrowseConfig();
    void onLoadConfig();
    void onSampleTableItemChanged(QTableWidgetItem* item);
    void onToggleTestMode(bool enabled);
    void onToggleRecording();
    void onI2cWrite();
    void onI2cRead();
    void onSpiWrite();
    void onSocPorToggled(bool checked);

private:
    bool sendDebugRequestAndWait(const DebugRequestPacket_t& request, DebugResponsePacket_t& response, int timeoutMs = 1000);
    bool sendDebugRequestAndWaitInSession(const DebugRequestPacket_t& request, DebugResponsePacket_t& response, int timeoutMs = 1000);
    bool sendI2cWriteInSession(uint8_t busId, uint8_t devAddr, uint32_t regAddr, uint8_t regLen, uint32_t value, uint8_t valueLen);
    void configureIna238ShuntCalibration(const PowersConfig& config);
    void configureInaSampleMode(const PowersConfig& config);
    uint16_t calculateIna238ShuntCal(const SampleConfig& sampleCfg) const;
    bool setSocPorLevel(bool high, bool updateCheckBox);
    void applyOnlineTargetVoltage(int powerId);
    QString currentConfigPath() const;
    QString defaultConfigPath() const;
    void addConfigPathOption(const QString& filePath);
    void setupSampleModeSelector();
    void setupDebugPanel();
    void setupControlPanel();
    void refreshSampleTypeSelector();
    void onSampleTypeChanged(int index);
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
    void waitCalibrationThread();
    QString calibrationFilePath() const;
    bool parseInputU8(QLineEdit* edit, uint8_t &value) const;
    bool parseInputU32(QLineEdit* edit, uint32_t &value) const;
    bool parseHexBytes(const QString& text, uint8_t* out, uint8_t &outLen) const;
    QString formatResponseValue(const DebugResponsePacket_t& response) const;
    void setI2cStatus(const QString& text);
    void setSpiStatus(const QString& text);
    void updateI2cValueBits(uint32_t value, uint8_t valueLen);

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
    QComboBox* sample_type_combo_ = nullptr;
    WaveformWidget* waveform_widget_ = nullptr;
    QAction* test_mode_action_ = nullptr;
    QPushButton* sample_toggle_button_ = nullptr;
    QPushButton* power_on_button_ = nullptr;
    QPushButton* calibration_toggle_button_ = nullptr;
    QPushButton* restore_defaults_button_ = nullptr;
    QPushButton* load_calibration_button_ = nullptr;
    QPushButton* record_button_ = nullptr;
    QCheckBox* soc_por_check_ = nullptr;
    std::unique_ptr<WaveformRecorder> waveform_recorder_;
    QComboBox* i2c_bus_combo_ = nullptr;
    QLineEdit* i2c_slave_id_edit_ = nullptr;
    QLineEdit* i2c_reg_addr_edit_ = nullptr;
    QComboBox* i2c_addr_len_combo_ = nullptr;
    QComboBox* i2c_value_len_combo_ = nullptr;
    QLineEdit* i2c_value_edit_ = nullptr;
    QLabel* i2c_read_value_label_ = nullptr;
    QLabel* i2c_status_label_ = nullptr;
    std::array<QCheckBox*, 16> i2c_value_bit_checks_{};
    QComboBox* spi_cs_combo_ = nullptr;
    QLineEdit* spi_tx_edit_ = nullptr;
    QLabel* spi_status_label_ = nullptr;
    bool power_on_completed_ = false;
    std::atomic<bool> power_on_running_{false};
    std::thread power_on_thread_;
    std::atomic<bool> calibration_running_{false};
    std::thread calibration_thread_;
    std::atomic<bool> test_mode_running_{false};
    std::thread test_mode_thread_;
};
#endif // MAINWINDOW_H
