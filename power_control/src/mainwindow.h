#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <array>
#include <memory>
#include <vector>

#include "power_config.h"
#include "usb_driver.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QTableWidgetItem;

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
    void onBrowseConfig();
    void onLoadConfig();
    void onSampleTableItemChanged(QTableWidgetItem* item);

private:
    QString currentConfigPath() const;
    QString defaultConfigPath() const;
    void addConfigPathOption(const QString& filePath);
    void refreshSampleTable();
    void onDataReceived(const uint8_t* data, int length);
    void updateSampleValuesFromPacket(const SampleDataPacket& packet);
    void appendLog(const QString& message);
    void refreshDeviceList();
    void updateConnectionState();

    Ui::MainWindow *ui;
    std::vector<USBDeviceInfo> devices_;
    PowersConfig power_configs_{};
    std::array<uint16_t, SAMPLE_DATA_COUNT> sampled_volt_{};
    std::array<uint16_t, SAMPLE_DATA_COUNT> sampled_curr_{};
    std::array<bool, SAMPLE_DATA_COUNT> has_sampled_volt_{};
    std::array<bool, SAMPLE_DATA_COUNT> has_sampled_curr_{};
    bool refreshing_sample_table_ = false;
    bool sampling_started_ = false;
    std::unique_ptr<USBDriver> usb_driver_;
};
#endif // MAINWINDOW_H
