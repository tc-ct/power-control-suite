#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QListWidgetItem>
#include <QMetaObject>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTableWidgetItem>
#include <cstring>

#include "file_parse.h"
#include "stm32_comm.h"

namespace {
constexpr uint16_t kDefaultVid = 0x0483;
constexpr uint16_t kDefaultPid = 0x5750;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->sampleTableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->sampleTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->sampleTableWidget->verticalHeader()->setVisible(false);

    connect(ui->queryButton, &QPushButton::clicked, this, &MainWindow::onQueryDevices);
    connect(ui->openButton, &QPushButton::clicked, this, &MainWindow::onToggleOpenDevice);
    connect(ui->sampleToggleButton, &QPushButton::clicked, this, &MainWindow::onToggleSampling);
    connect(ui->browseConfigButton, &QPushButton::clicked, this, &MainWindow::onBrowseConfig);
    connect(ui->loadConfigButton, &QPushButton::clicked, this, &MainWindow::onLoadConfig);
    connect(ui->sampleTableWidget, &QTableWidget::itemChanged, this, &MainWindow::onSampleTableItemChanged);

    const QString configPath = defaultConfigPath();
    addConfigPathOption(configPath);

    updateConnectionState();
    if (QFileInfo::exists(configPath)) {
        onLoadConfig();
    } else {
        appendLog(QStringLiteral("默认配置文件未找到: %1").arg(QDir::toNativeSeparators(configPath)));
    }
    onQueryDevices();
}

MainWindow::~MainWindow()
{
    if (usb_driver_) {
        usb_driver_->close();
    }

    delete ui;
}

QString MainWindow::currentConfigPath() const {
    return ui->configPathComboBox->currentText().trimmed();
}

QString MainWindow::defaultConfigPath() const {
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config/power_config.json"));
}

void MainWindow::addConfigPathOption(const QString& filePath) {
    const QString normalizedPath = QDir::toNativeSeparators(filePath.trimmed());
    if (normalizedPath.isEmpty()) {
        return;
    }

    const int index = ui->configPathComboBox->findText(normalizedPath);
    if (index < 0) {
        ui->configPathComboBox->addItem(normalizedPath);
        ui->configPathComboBox->setCurrentIndex(ui->configPathComboBox->count() - 1);
        return;
    }

    ui->configPathComboBox->setCurrentIndex(index);
}

void MainWindow::appendLog(const QString& message) {
    ui->logEdit->appendPlainText(message);
    statusBar()->showMessage(message, 3000);
}

void MainWindow::refreshSampleTable() {
    const QSignalBlocker blocker(ui->sampleTableWidget);
    refreshing_sample_table_ = true;
    ui->sampleTableWidget->setRowCount(SAMPLE_DATA_COUNT);

    for (int sampleId = 0; sampleId < SAMPLE_DATA_COUNT; ++sampleId) {
        const SampleConfig& cfg = power_configs_.sample_cfg[sampleId];
        const QString name = cfg.name[0] == '\0'
            ? QStringLiteral("Sample %1").arg(sampleId)
            : QString::fromLocal8Bit(cfg.name);
        ui->sampleTableWidget->setItem(sampleId, 0, new QTableWidgetItem(QString::number(sampleId)));
        ui->sampleTableWidget->setItem(sampleId, 1, new QTableWidgetItem(name));

        QTableWidgetItem* voltEnItem = new QTableWidgetItem();
        voltEnItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        voltEnItem->setCheckState(cfg.volt_en ? Qt::Checked : Qt::Unchecked);
        ui->sampleTableWidget->setItem(sampleId, 2, voltEnItem);

        QTableWidgetItem* currEnItem = new QTableWidgetItem();
        currEnItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        currEnItem->setCheckState(cfg.current_en ? Qt::Checked : Qt::Unchecked);
        ui->sampleTableWidget->setItem(sampleId, 4, currEnItem);

        const QString voltText = has_sampled_volt_[sampleId]
            ? QString::number(sampled_volt_[sampleId])
            : QStringLiteral("--");
        const QString currText = has_sampled_curr_[sampleId]
            ? QString::number(sampled_curr_[sampleId])
            : QStringLiteral("--");
        ui->sampleTableWidget->setItem(sampleId, 3, new QTableWidgetItem(voltText));
        ui->sampleTableWidget->setItem(sampleId, 5, new QTableWidgetItem(currText));
    }
    refreshing_sample_table_ = false;
}

void MainWindow::onSampleTableItemChanged(QTableWidgetItem* item) {
    if (!item || refreshing_sample_table_) {
        return;
    }

    const int row = item->row();
    if (row < 0 || row >= SAMPLE_DATA_COUNT) {
        return;
    }

    if (item->column() == 2) {
        power_configs_.sample_cfg[row].volt_en = (item->checkState() == Qt::Checked);
    } else if (item->column() == 4) {
        power_configs_.sample_cfg[row].current_en = (item->checkState() == Qt::Checked);
    }
}

void MainWindow::onDataReceived(const uint8_t* data, int length) {
    if (!data || length < static_cast<int>(sizeof(SampleDataPacket))) {
        return;
    }

    SampleDataPacket packet{};
    memcpy(&packet, data, sizeof(SampleDataPacket));

    QMetaObject::invokeMethod(this, [this, packet]() {
        updateSampleValuesFromPacket(packet);
    }, Qt::QueuedConnection);
}

void MainWindow::updateSampleValuesFromPacket(const SampleDataPacket& packet) {
    if (packet.type == I2C_DATA_VBUS) {
        for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
            sampled_volt_[i] = packet.channel_volt_mv[i];
            has_sampled_volt_[i] = true;
        }
    } else if (packet.type == I2C_DATA_CURRENT) {
        for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
            sampled_curr_[i] = packet.channel_curr_ma[i];
            has_sampled_curr_[i] = true;
        }
    } else {
        return;
    }

    const QSignalBlocker blocker(ui->sampleTableWidget);
    for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
        if (QTableWidgetItem* voltItem = ui->sampleTableWidget->item(i, 3)) {
            voltItem->setText(has_sampled_volt_[i] ? QString::number(sampled_volt_[i]) : QStringLiteral("--"));
        }
        if (QTableWidgetItem* currItem = ui->sampleTableWidget->item(i, 5)) {
            currItem->setText(has_sampled_curr_[i] ? QString::number(sampled_curr_[i]) : QStringLiteral("--"));
        }
    }
}

void MainWindow::refreshDeviceList() {
    ui->deviceList->clear();
    ui->deviceComboBox->clear();

    for (const USBDeviceInfo& device : devices_) {
        const QString product = device.product.empty()
            ? QStringLiteral("Unknown")
            : QString::fromWCharArray(device.product.c_str());
        const QString manufacturer = device.manufacturer.empty()
            ? QStringLiteral("Unknown")
            : QString::fromWCharArray(device.manufacturer.c_str());
        const QString displayName = product != QStringLiteral("Unknown")
            ? product
            : manufacturer;
        const QString path = QString::fromLocal8Bit(device.path.c_str());
        const QString text = QStringLiteral("%1").arg(displayName);

        QListWidgetItem* item = new QListWidgetItem(text, ui->deviceList);
        item->setData(Qt::UserRole, path);

        ui->deviceComboBox->addItem(text, path);
    }

    if (!devices_.empty()) {
        ui->deviceList->setCurrentRow(0);
        ui->deviceComboBox->setCurrentIndex(0);
    }
}

void MainWindow::updateConnectionState() {
    const bool isOpen = usb_driver_ && usb_driver_->isOpen();
    ui->openButton->setText(isOpen ? QStringLiteral("关闭设备") : QStringLiteral("打开设备"));
    ui->statusValueLabel->setText(isOpen ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    ui->sampleToggleButton->setEnabled(isOpen);

    if (!isOpen) {
        sampling_started_ = false;
    }
    ui->sampleToggleButton->setText(sampling_started_ ? QStringLiteral("停止采样") : QStringLiteral("开始采样"));
}

void MainWindow::onQueryDevices() {
    devices_ = USBDriver::queryDevices(kDefaultVid, kDefaultPid);
    refreshDeviceList();

    appendLog(QStringLiteral("查询完成: 共 %1 个设备")
                  .arg(devices_.size()));
}

void MainWindow::onToggleOpenDevice() {
    if (usb_driver_ && usb_driver_->isOpen()) {
        if (sampling_started_) {
            if(power_configs_.volt_sample_en) SendStopSample(*usb_driver_, SAMPLE_TYPE_VOLTAGE);
            if(power_configs_.curr_sample_en) SendStopSample(*usb_driver_, SAMPLE_TYPE_CURRENT);
            sampling_started_ = false;
        }
        usb_driver_->close();
        appendLog(QStringLiteral("设备已关闭"));
        updateConnectionState();
        return;
    }

    if (devices_.empty()) {
        onQueryDevices();
        if (devices_.empty()) {
            QMessageBox::information(this, QStringLiteral("未发现设备"), QStringLiteral("当前默认配置下没有可打开的设备。"));
            return;
        }
    }

    QString path;
    if (ui->deviceComboBox->currentIndex() >= 0) {
        path = ui->deviceComboBox->currentData().toString();
    } else {
        QListWidgetItem* currentItem = ui->deviceList->currentItem();
        if (currentItem) {
            path = currentItem->data(Qt::UserRole).toString();
        } else {
            path = QString::fromLocal8Bit(devices_.front().path.c_str());
        }
    }

    usb_driver_ = std::make_unique<USBDriver>(kDefaultVid, kDefaultPid);
    if (!usb_driver_->open(path.toLocal8Bit().constData())) {
        appendLog(QStringLiteral("打开设备失败: %1").arg(path));
        updateConnectionState();
        return;
    }

    usb_driver_->setReceiveCallback([this](const uint8_t* data, int length) {
        onDataReceived(data, length);
    });

    appendLog(QStringLiteral("设备已打开: %1").arg(path));
    updateConnectionState();
}

void MainWindow::onToggleSampling() {
    if (!usb_driver_ || !usb_driver_->isOpen()) {
        QMessageBox::information(this, QStringLiteral("未连接设备"), QStringLiteral("请先打开设备后再开始采样。"));
        return;
    }

    if (!sampling_started_) {
        if(power_configs_.volt_sample_en) SendStartSample(*usb_driver_, SAMPLE_TYPE_VOLTAGE);
        if(power_configs_.curr_sample_en) SendStartSample(*usb_driver_, SAMPLE_TYPE_CURRENT);
        sampling_started_ = true;
        appendLog(QStringLiteral("开始采样"));
    } else {
        if(power_configs_.volt_sample_en) SendStopSample(*usb_driver_, SAMPLE_TYPE_VOLTAGE);
        if(power_configs_.curr_sample_en) SendStopSample(*usb_driver_, SAMPLE_TYPE_CURRENT);
        sampling_started_ = false;
        appendLog(QStringLiteral("停止采样"));
    }

    updateConnectionState();
}

void MainWindow::onBrowseConfig() {
    const QString initialPath = currentConfigPath().isEmpty() ? defaultConfigPath() : currentConfigPath();
    const QString selectedFile = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择配置文件"),
        QFileInfo(initialPath).absolutePath(),
        QStringLiteral("JSON Files (*.json);;All Files (*.*)"));

    if (selectedFile.isEmpty()) {
        return;
    }

    addConfigPathOption(selectedFile);
}

void MainWindow::onLoadConfig() {
    const QString filePath = QDir::fromNativeSeparators(currentConfigPath());
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("配置文件为空"), QStringLiteral("请先选择 power_config.json 文件。"));
        return;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        QMessageBox::warning(this, QStringLiteral("配置文件不存在"), QStringLiteral("找不到配置文件: %1").arg(QDir::toNativeSeparators(filePath)));
        appendLog(QStringLiteral("配置加载失败: 文件不存在 %1").arg(QDir::toNativeSeparators(filePath)));
        return;
    }

    PowersConfig loadedConfig{};
    const QByteArray pathBytes = filePath.toLocal8Bit();
    if (!LoadPowerConfig(pathBytes.constData(), &loadedConfig)) {
        QMessageBox::warning(this, QStringLiteral("配置加载失败"), QStringLiteral("power_config.json 解析失败，请检查文件格式。"));
        appendLog(QStringLiteral("配置加载失败: %1").arg(QDir::toNativeSeparators(filePath)));
        return;
    }

    power_configs_ = loadedConfig;
    refreshSampleTable();
    addConfigPathOption(filePath);
    appendLog(QStringLiteral("配置加载成功: %1").arg(QDir::toNativeSeparators(filePath)));
}
