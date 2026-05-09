#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QBoxLayout>
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QMetaObject>
#include <QEventLoop>
#include <QMessageBox>
#include <QMenuBar>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionButton>
#include <QStringList>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <thread>

#include "config_service.h"
#include "device_session_service.h"
#include "adc_def.h"
#include "power_control.h"
#include "sample_channel_map.h"
#include "stm32_comm.h"
#include "waveform_recorder.h"
#include "waveform_widget.h"

namespace {
constexpr uint16_t kDefaultVid = 0x0483;
constexpr uint16_t kDefaultPid = 0x5750;
constexpr uint8_t kIna238ConfigRegAddr = 0x00;
constexpr uint8_t kIna238AdcConfigRegAddr = 0x01;
constexpr uint8_t kIna238ShuntCalRegAddr = 0x02;
constexpr uint8_t kIna260ConfigRegAddr = 0x00;
constexpr uint8_t kIna260DevAddr = 0x44;
constexpr uint16_t kIna238ConfigValue = 0x0010;
constexpr uint16_t kIna238AdcConfigVoltageOnly = 0x9494;
constexpr uint16_t kIna238AdcConfigCurrentOnly = 0xA494;
constexpr uint16_t kIna238AdcConfigVoltageCurrent = 0xB494;
constexpr uint16_t kIna260ConfigVoltageOnly = 0x6806;
constexpr uint16_t kIna260ConfigCurrentOnly = 0x6805;
constexpr uint16_t kIna260ConfigVoltageCurrent = 0x6807;
constexpr int kSocPorPortGpioD = 2;
constexpr int kSocPorPinPd2 = 2;
constexpr int kSocPorReleaseDelayMs = 1;
constexpr int kDebugTimeoutMs = 1000;
constexpr int kIna238ShuntCalRetryCount = 1;
constexpr int kIna238ShuntCalWriteGapMs = 5;
constexpr std::array<uint8_t, I2C1_INA238_NUM> kI2c1Ina238DevAddrs = {
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C
};
constexpr std::array<uint8_t, I2C2_INA238_NUM> kI2c2Ina238DevAddrs = {
    0x40, 0x41, 0x42, 0x43, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D
};
constexpr std::array<int, SAMPLE_DATA_COUNT> kSampleDisplayToPowerId = {
    0, 1, 2, 3, 4, 5,
    6, 6,
    7, 7,
    8, 8,
    9,
    10, 11, 12,
    13,
    14, 14, 14,
    15, 15, 15, 15, 15,
    16, 17
};

QString buildDeviceDisplayName(const USBDeviceInfo& device) {
    const QString product = device.product.empty()
        ? QStringLiteral("Unknown")
        : QString::fromWCharArray(device.product.c_str());
    const QString manufacturer = device.manufacturer.empty()
        ? QStringLiteral("Unknown")
        : QString::fromWCharArray(device.manufacturer.c_str());
    return product != QStringLiteral("Unknown") ? product : manufacturer;
}

SampleDataPacketTF reorderPacketToDisplayOrder(const SampleDataPacketTF& rawPacket) {
    SampleDataPacketTF displayPacket{};
    displayPacket.timestamp = rawPacket.timestamp;
    for (int displayId = 0; displayId < SAMPLE_DATA_COUNT; ++displayId) {
        const int rawIndex = sample_channel_map::displayToRaw(displayId);
        if (rawIndex < 0 || rawIndex >= SAMPLE_DATA_COUNT) {
            continue;
        }
        displayPacket.channel_volt_mv[displayId] = rawPacket.channel_volt_mv[rawIndex];
        displayPacket.channel_curr_ma[displayId] = rawPacket.channel_curr_ma[rawIndex];
    }
    return displayPacket;
}

QString formatVoltageReading(float millivolts) {
    return QString::number(static_cast<double>(millivolts) / 1000.0, 'f', 3);
}

QString formatCurrentReading(float milliamps) {
    if (!std::isfinite(milliamps)) {
        return QStringLiteral("--");
    }

    const double absValue = std::abs(static_cast<double>(milliamps));
    if (absValue < 1e-12) {
        return QStringLiteral("0.000");
    }

    const int digitsBeforeDecimal = static_cast<int>(std::floor(std::log10(absValue))) + 1;
    const int decimals = std::max(0, 4 - digitsBeforeDecimal);
    return QString::number(static_cast<double>(milliamps), 'f', decimals);
}

int powerIdForSampleDisplayId(int sampleDisplayId) {
    if (sampleDisplayId < 0 || sampleDisplayId >= static_cast<int>(kSampleDisplayToPowerId.size())) {
        return -1;
    }
    return kSampleDisplayToPowerId[sampleDisplayId];
}

bool isPowerTargetSampleDisplay(const PowersConfig& config, int sampleDisplayId, int powerId) {
    if (powerId < 0 || powerId >= POWER_SUPPLY_COUNT) {
        return false;
    }

    const int targetDisplayId = sample_channel_map::rawToDisplay(config.supplies[powerId].means_pt);
    return targetDisplayId >= 0 && targetDisplayId == sampleDisplayId;
}

bool isEditableTargetVoltageRow(
    const PowersConfig& config,
    const SampleTableRowMapping& mapping,
    SampleTableDisplayMode mode) {
    if (!mapping.valid || mapping.power_id < 0 || mapping.power_id >= POWER_SUPPLY_COUNT) {
        return false;
    }
    if (mode == SampleTableDisplayMode::PowerId) {
        return true;
    }
    return isPowerTargetSampleDisplay(config, mapping.sample_id, mapping.power_id);
}

class CenteredCheckBoxDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem viewOption(option);
        initStyleOption(&viewOption, index);
        viewOption.text.clear();
        viewOption.icon = QIcon();
        viewOption.features &= ~QStyleOptionViewItem::HasCheckIndicator;

        const QWidget* widget = viewOption.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &viewOption, painter, widget);

        QStyleOptionButton checkBoxOption;
        checkBoxOption.state |= QStyle::State_Enabled;
        checkBoxOption.state |= (index.data(Qt::CheckStateRole).toInt() == Qt::Checked)
            ? QStyle::State_On
            : QStyle::State_Off;
        if (option.state & QStyle::State_MouseOver) {
            checkBoxOption.state |= QStyle::State_MouseOver;
        }

        const QRect indicatorRect = style->subElementRect(QStyle::SE_CheckBoxIndicator, &checkBoxOption, widget);
        checkBoxOption.rect = QStyle::alignedRect(
            option.direction,
            Qt::AlignCenter,
            indicatorRect.size(),
            option.rect);
        style->drawControl(QStyle::CE_CheckBox, &checkBoxOption, painter, widget);
    }

    bool editorEvent(
        QEvent* event,
        QAbstractItemModel* model,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) override {
        const Qt::ItemFlags flags = index.flags();
        if (!(flags & Qt::ItemIsUserCheckable) || !(flags & Qt::ItemIsEnabled)) {
            return false;
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            QStyleOptionButton checkBoxOption;
            const QWidget* widget = option.widget;
            QStyle* style = widget ? widget->style() : QApplication::style();
            const QRect indicatorRect = style->subElementRect(QStyle::SE_CheckBoxIndicator, &checkBoxOption, widget);
            const QRect alignedRect = QStyle::alignedRect(
                option.direction,
                Qt::AlignCenter,
                indicatorRect.size(),
                option.rect);
            if (!alignedRect.contains(mouseEvent->position().toPoint())) {
                return false;
            }
        } else if (event->type() == QEvent::KeyPress) {
            const auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() != Qt::Key_Space && keyEvent->key() != Qt::Key_Select) {
                return false;
            }
        } else {
            return false;
        }

        const Qt::CheckState nextState = index.data(Qt::CheckStateRole).toInt() == Qt::Checked
            ? Qt::Unchecked
            : Qt::Checked;
        return model->setData(index, nextState, Qt::CheckStateRole);
    }
};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    session_service_ = std::make_unique<DeviceSessionService>(kDefaultVid, kDefaultPid, this);
    config_service_ = std::make_unique<ConfigService>();
    waveform_recorder_ = std::make_unique<WaveformRecorder>();
    connect(
        session_service_.get(),
        &DeviceSessionService::samplePacketReceived,
        this,
        &MainWindow::onDataReceived);

    ui->setupUi(this);
    resize(1280, 760);

    test_mode_action_ = menuBar()->addAction(QStringLiteral("测试模式"));
    test_mode_action_->setCheckable(true);
    connect(test_mode_action_, &QAction::toggled, this, &MainWindow::onToggleTestMode);
    sample_toggle_button_ = new QPushButton(QStringLiteral("开始采样"), this);
    sample_toggle_button_->setEnabled(false);
    connect(sample_toggle_button_, &QPushButton::clicked, this, &MainWindow::onToggleSampling);
    power_on_button_ = new QPushButton(QStringLiteral("POWER ON/OFF"), this);
    connect(power_on_button_, &QPushButton::clicked, this, &MainWindow::onPowerOnClicked);
    calibration_toggle_button_ = new QPushButton(this);
    connect(calibration_toggle_button_, &QPushButton::clicked, this, &MainWindow::onStartCalibrationClicked);
    record_button_ = new QPushButton(QStringLiteral("开始录制"), this);
    record_button_->setEnabled(false);
    connect(record_button_, &QPushButton::clicked, this, &MainWindow::onToggleRecording);
    setupControlPanel();
    refreshSampleTypeSelector();

    const int buttonWidth = qMax(
        112,
        qMax(
            qMax(ui->browseConfigButton->sizeHint().width(), ui->loadConfigButton->sizeHint().width()),
            qMax(ui->queryButton->sizeHint().width(), ui->openButton->sizeHint().width())));
    const int buttonHeight = 24;
    for (QPushButton* button : {ui->browseConfigButton, ui->loadConfigButton, ui->queryButton, ui->openButton}) {
        button->setMinimumWidth(buttonWidth);
        button->setMinimumHeight(buttonHeight);
    }
    refreshCalibrationButtonText();

    const int leftLabelWidth = qMax(ui->configPathLabel->sizeHint().width(), ui->deviceNameLabel->sizeHint().width());
    ui->configPathLabel->setMinimumWidth(leftLabelWidth);
    ui->deviceNameLabel->setMinimumWidth(leftLabelWidth);

    auto* waveformLayout = new QVBoxLayout(ui->waveformHost);
    waveformLayout->setContentsMargins(0, 0, 0, 0);
    waveform_widget_ = new WaveformWidget(ui->waveformHost);
    waveformLayout->addWidget(waveform_widget_);
    setupDebugPanel();

    if (QBoxLayout* mainLayout = qobject_cast<QBoxLayout*>(ui->centralwidget->layout())) {
        mainLayout->setStretch(0, 0);
        mainLayout->setStretch(1, 1);
        mainLayout->setStretch(2, 0);
    }

    ui->sampleGroupBox->setMinimumWidth(580);
    ui->sampleTableWidget->setMinimumHeight(560);

    QHeaderView* sampleHeader = ui->sampleTableWidget->horizontalHeader();
    if (QTableWidgetItem* headerItem = ui->sampleTableWidget->horizontalHeaderItem(2)) {
        headerItem->setText(QStringLiteral("设定值(V)"));
    }
    if (QTableWidgetItem* headerItem = ui->sampleTableWidget->horizontalHeaderItem(3)) {
        headerItem->setText(QStringLiteral("显示"));
    }
    if (QTableWidgetItem* headerItem = ui->sampleTableWidget->horizontalHeaderItem(4)) {
        headerItem->setText(QStringLiteral("电压(V)"));
    }
    if (QTableWidgetItem* headerItem = ui->sampleTableWidget->horizontalHeaderItem(5)) {
        headerItem->setText(QStringLiteral("显示"));
    }
    if (QTableWidgetItem* headerItem = ui->sampleTableWidget->horizontalHeaderItem(6)) {
        headerItem->setText(QStringLiteral("电流(mA)"));
    }
    sampleHeader->setStretchLastSection(false);
    sampleHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
    sampleHeader->setSectionResizeMode(1, QHeaderView::Stretch);
    sampleHeader->setSectionResizeMode(2, QHeaderView::Fixed);
    sampleHeader->setSectionResizeMode(3, QHeaderView::Fixed);
    sampleHeader->setSectionResizeMode(4, QHeaderView::Fixed);
    sampleHeader->setSectionResizeMode(5, QHeaderView::Fixed);
    sampleHeader->setSectionResizeMode(6, QHeaderView::Fixed);
    ui->sampleTableWidget->setColumnWidth(2, 84);
    ui->sampleTableWidget->setColumnWidth(3, 54);
    ui->sampleTableWidget->setColumnWidth(4, 78);
    ui->sampleTableWidget->setColumnWidth(5, 54);
    ui->sampleTableWidget->setColumnWidth(6, 86);
    ui->sampleTableWidget->verticalHeader()->setVisible(false);
    ui->sampleTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->sampleTableWidget->verticalHeader()->setDefaultSectionSize(20);
    ui->sampleTableWidget->verticalHeader()->setMinimumSectionSize(20);
    ui->sampleTableWidget->setAlternatingRowColors(true);
    ui->sampleTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->sampleTableWidget->setItemDelegateForColumn(3, new CenteredCheckBoxDelegate(ui->sampleTableWidget));
    ui->sampleTableWidget->setItemDelegateForColumn(5, new CenteredCheckBoxDelegate(ui->sampleTableWidget));
    ui->sampleTableWidget->setStyleSheet(
        "QTableWidget {"
        "   font-size: 11px;"
        "   gridline-color: #d9dee7;"
        "   alternate-background-color: #f7f9fc;"
        "   selection-background-color: #cfe3ff;"
        "   selection-color: #1f2937;"
        "}"
        "QHeaderView::section {"
        "   font-size: 15px;"
        "   font-weight: 600;"
        "   color: #334155;"
        "   background-color: #eef2f7;"
        "   border: 0px;"
        "   border-bottom: 1px solid #d9dee7;"
        "   padding: 1px 6px;"
        "}"
        "QTableWidget::item {"
        "   padding: 1px 6px;"
        "}"
    );
    for (int column : {0, 2, 3, 4, 5, 6}) {
        if (QTableWidgetItem* headerItem = ui->sampleTableWidget->horizontalHeaderItem(column)) {
            headerItem->setTextAlignment(Qt::AlignCenter);
        }
    }
    setupSampleModeSelector();

    connect(ui->queryButton, &QPushButton::clicked, this, &MainWindow::onQueryDevices);
    connect(ui->openButton, &QPushButton::clicked, this, &MainWindow::onToggleOpenDevice);
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
    stopRecording(false);
    waitCalibrationThread();
    waitPowerOnThread();
    stopTestModeThread();
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

void MainWindow::setupSampleModeSelector() {
    if (!ui->sampleModeHostLayout) {
        return;
    }

    auto* modeLabel = new QLabel(QStringLiteral("显示模式"), ui->sampleGroupBox);
    sample_mode_combo_ = new QComboBox(ui->sampleGroupBox);
    sample_mode_combo_->addItem(QStringLiteral("按电源ID"), static_cast<int>(SampleTableDisplayMode::PowerId));
    sample_mode_combo_->addItem(QStringLiteral("按采样通道"), static_cast<int>(SampleTableDisplayMode::SampleChannel));
    sample_mode_combo_->setCurrentIndex(0);

    ui->sampleModeHostLayout->insertWidget(0, modeLabel);
    ui->sampleModeHostLayout->insertWidget(1, sample_mode_combo_);

    connect(sample_mode_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (!sample_mode_combo_ || index < 0) {
            return;
        }
        const SampleTableDisplayMode mode = static_cast<SampleTableDisplayMode>(sample_mode_combo_->itemData(index).toInt());
        if (mode == sample_table_mode_) {
            return;
        }
        sample_table_mode_ = mode;
        rebuildSampleRowMap();
        refreshSampleTable();
    });
}

void MainWindow::setupControlPanel() {
    auto* ctrlLayout = new QGridLayout(ui->ctrlHost);
    ctrlLayout->setContentsMargins(8, 8, 8, 8);
    ctrlLayout->setHorizontalSpacing(12);
    ctrlLayout->setVerticalSpacing(10);

    constexpr int kControlButtonWidth = 150;
    constexpr int kControlButtonHeight = 34;
    const QSize controlButtonSize(kControlButtonWidth, kControlButtonHeight);

    restore_defaults_button_ = new QPushButton(QStringLiteral("恢复默认参数"), ui->ctrlHost);
    connect(restore_defaults_button_, &QPushButton::clicked, this, &MainWindow::onRestoreDefaultValues);
    load_calibration_button_ = new QPushButton(QStringLiteral("加载校准值"), ui->ctrlHost);
    connect(load_calibration_button_, &QPushButton::clicked, this, &MainWindow::onLoadCalibrationValues);
    soc_por_check_ = new QCheckBox(QStringLiteral("SOC POR"), ui->ctrlHost);
    soc_por_check_->setEnabled(false);
    connect(soc_por_check_, &QCheckBox::toggled, this, &MainWindow::onSocPorToggled);
    sample_type_combo_ = new QComboBox(ui->ctrlHost);
    sample_type_combo_->addItem(QStringLiteral("电压+电流"), QVariant::fromValue(0x03));
    sample_type_combo_->addItem(QStringLiteral("仅电压"), QVariant::fromValue(0x01));
    sample_type_combo_->addItem(QStringLiteral("仅电流"), QVariant::fromValue(0x02));
    sample_type_combo_->addItem(QStringLiteral("都不采样"), QVariant::fromValue(0x00));
    connect(sample_type_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::onSampleTypeChanged);

    sample_toggle_button_->setParent(ui->ctrlHost);
    calibration_toggle_button_->setParent(ui->ctrlHost);
    record_button_->setParent(ui->ctrlHost);
    power_on_button_->setParent(ui->ctrlHost);

    for (QWidget* widget : {
             static_cast<QWidget*>(sample_toggle_button_),
             static_cast<QWidget*>(calibration_toggle_button_),
             static_cast<QWidget*>(restore_defaults_button_),
             static_cast<QWidget*>(load_calibration_button_),
             static_cast<QWidget*>(record_button_),
             static_cast<QWidget*>(sample_type_combo_)}) {
        widget->setMinimumSize(controlButtonSize);
        widget->setMaximumSize(controlButtonSize);
    }

    power_on_button_->setMinimumSize(126, 78);
    power_on_button_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    ctrlLayout->addWidget(calibration_toggle_button_, 0, 0);
    ctrlLayout->addWidget(sample_toggle_button_, 0, 1);
    ctrlLayout->addWidget(restore_defaults_button_, 0, 2);
    ctrlLayout->addWidget(load_calibration_button_, 1, 0);
    ctrlLayout->addWidget(record_button_, 1, 1);
    ctrlLayout->addWidget(sample_type_combo_, 1, 2);
    ctrlLayout->addWidget(soc_por_check_, 0, 3, 1, 1, Qt::AlignCenter);
    ctrlLayout->addWidget(power_on_button_, 1, 3, 1, 1);
    ctrlLayout->setColumnMinimumWidth(0, kControlButtonWidth);
    ctrlLayout->setColumnMinimumWidth(1, kControlButtonWidth);
    ctrlLayout->setColumnMinimumWidth(2, kControlButtonWidth);
}

void MainWindow::refreshSampleTypeSelector() {
    if (!sample_type_combo_ || !config_service_) {
        return;
    }

    const PowersConfig& config = config_service_->config();
    const int mask = (config.volt_sample_en ? 0x01 : 0x00) | (config.curr_sample_en ? 0x02 : 0x00);
    const int index = sample_type_combo_->findData(mask);
    const QSignalBlocker blocker(sample_type_combo_);
    sample_type_combo_->setCurrentIndex(index >= 0 ? index : 0);
}

void MainWindow::onSampleTypeChanged(int index) {
    if (!sample_type_combo_ || !config_service_ || index < 0) {
        return;
    }

    const int mask = sample_type_combo_->itemData(index).toInt();
    PowersConfig& config = config_service_->mutableConfig();
    config.volt_sample_en = (mask & 0x01) != 0;
    config.curr_sample_en = (mask & 0x02) != 0;
    appendLog(QStringLiteral("采样模式: %1").arg(sample_type_combo_->currentText()));
}

void MainWindow::setupDebugPanel() {
    auto* debugLayout = new QVBoxLayout(ui->debugHost);
    if (!debugLayout) {
        return;
    }
    debugLayout->setContentsMargins(0, 0, 0, 0);
    debugLayout->setSpacing(0);

    auto* tabs = new QTabWidget(ui->debugHost);
    debugLayout->addWidget(tabs);

    auto* i2cPage = new QWidget(tabs);
    auto* i2cGrid = new QGridLayout(i2cPage);
    i2cGrid->setContentsMargins(8, 8, 8, 8);
    i2cGrid->setHorizontalSpacing(10);
    i2cGrid->setVerticalSpacing(6);

    i2c_slave_id_edit_ = new QLineEdit(i2cPage);
    i2c_slave_id_edit_->setPlaceholderText(QStringLiteral("0x40"));
    i2c_slave_id_edit_->setText(QStringLiteral("0x40"));
    i2c_slave_id_edit_->setMaximumWidth(110);

    i2c_reg_addr_edit_ = new QLineEdit(i2cPage);
    i2c_reg_addr_edit_->setPlaceholderText(QStringLiteral("0x00"));
    i2c_reg_addr_edit_->setText(QStringLiteral("0x00"));
    i2c_reg_addr_edit_->setMaximumWidth(120);

    i2c_bus_combo_ = new QComboBox(i2cPage);
    i2c_bus_combo_->addItem(QStringLiteral("I2C1"), DEBUG_BUS_I2C1);
    i2c_bus_combo_->addItem(QStringLiteral("I2C2"), DEBUG_BUS_I2C2);
    i2c_bus_combo_->setMaximumWidth(90);

    i2c_addr_len_combo_ = new QComboBox(i2cPage);
    for (int i = 1; i <= 4; ++i) {
        i2c_addr_len_combo_->addItem(QString::number(i), i);
    }
    i2c_addr_len_combo_->setMaximumWidth(70);

    i2c_value_len_combo_ = new QComboBox(i2cPage);
    for (int i = 1; i <= 4; ++i) {
        i2c_value_len_combo_->addItem(QString::number(i), i);
    }
    i2c_value_len_combo_->setCurrentIndex(1);
    i2c_value_len_combo_->setMaximumWidth(70);

    i2c_value_edit_ = new QLineEdit(i2cPage);
    i2c_value_edit_->setPlaceholderText(QStringLiteral("0x0000"));
    i2c_value_edit_->setMaximumWidth(120);

    auto* i2cReadButton = new QPushButton(QStringLiteral("Read"), i2cPage);
    auto* i2cWriteButton = new QPushButton(QStringLiteral("Write"), i2cPage);
    i2c_read_value_label_ = new QLabel(QStringLiteral("--"), i2cPage);
    i2c_status_label_ = new QLabel(QStringLiteral("Idle"), i2cPage);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Status"), i2cPage), 0, 0);
    i2cGrid->addWidget(i2c_status_label_, 0, 1, 1, 7);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Slave ID(7bit)"), i2cPage), 1, 0);
    i2cGrid->addWidget(i2c_slave_id_edit_, 1, 1);
    i2cGrid->addWidget(new QLabel(QStringLiteral("Reg Addr"), i2cPage), 1, 2);
    i2cGrid->addWidget(i2c_reg_addr_edit_, 1, 3);
    i2cGrid->addWidget(new QLabel(QStringLiteral("Bus"), i2cPage), 1, 4);
    i2cGrid->addWidget(i2c_bus_combo_, 1, 5);
    i2cGrid->addWidget(new QLabel(QStringLiteral("Addr Len"), i2cPage), 1, 6);
    i2cGrid->addWidget(i2c_addr_len_combo_, 1, 7);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Value"), i2cPage), 2, 0);
    i2cGrid->addWidget(i2c_value_edit_, 2, 1);
    i2cGrid->addWidget(new QLabel(QStringLiteral("Value Len"), i2cPage), 2, 2);
    i2cGrid->addWidget(i2c_value_len_combo_, 2, 3);
    i2cGrid->addWidget(new QLabel(QStringLiteral("Read Value"), i2cPage), 2, 4);
    i2cGrid->addWidget(i2c_read_value_label_, 2, 5);
    i2cGrid->addWidget(i2cReadButton, 2, 6);
    i2cGrid->addWidget(i2cWriteButton, 2, 7);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Value bits"), i2cPage), 3, 0);
    auto* bitLayout = new QHBoxLayout();
    bitLayout->setSpacing(4);
    for (int bit = 15; bit >= 0; --bit) {
        auto* bitColumn = new QVBoxLayout();
        bitColumn->setSpacing(0);
        auto* bitLabel = new QLabel(QString::number(bit), i2cPage);
        bitLabel->setAlignment(Qt::AlignCenter);
        auto* check = new QCheckBox(i2cPage);
        check->setEnabled(false);
        check->setFixedWidth(18);
        i2c_value_bit_checks_[bit] = check;
        bitColumn->addWidget(bitLabel);
        bitColumn->addWidget(check, 0, Qt::AlignCenter);
        bitLayout->addLayout(bitColumn);
    }
    bitLayout->addStretch(1);
    i2cGrid->addLayout(bitLayout, 3, 1, 1, 7);
    i2cGrid->setColumnStretch(8, 1);

    connect(i2cReadButton, &QPushButton::clicked, this, &MainWindow::onI2cRead);
    connect(i2cWriteButton, &QPushButton::clicked, this, &MainWindow::onI2cWrite);

    auto* spiPage = new QWidget(tabs);
    auto* spiGrid = new QGridLayout(spiPage);
    spiGrid->setContentsMargins(8, 8, 8, 8);
    spiGrid->setHorizontalSpacing(10);
    spiGrid->setVerticalSpacing(6);

    spi_cs_combo_ = new QComboBox(spiPage);
    spi_cs_combo_->addItem(QStringLiteral("CS0 (PA4)"), 0);
    spi_cs_combo_->addItem(QStringLiteral("CS1 (PA3)"), 1);
    spi_cs_combo_->addItem(QStringLiteral("CS2 (PA2)"), 2);

    spi_tx_edit_ = new QLineEdit(spiPage);
    spi_tx_edit_->setPlaceholderText(QStringLiteral("08 00 00 00"));

    auto* spiSendButton = new QPushButton(QStringLiteral("Send"), spiPage);
    spi_status_label_ = new QLabel(QStringLiteral("Idle"), spiPage);

    spiGrid->addWidget(new QLabel(QStringLiteral("Status"), spiPage), 0, 0);
    spiGrid->addWidget(spi_status_label_, 0, 1, 1, 3);
    spiGrid->addWidget(new QLabel(QStringLiteral("Chip Select"), spiPage), 1, 0);
    spiGrid->addWidget(spi_cs_combo_, 1, 1);
    spiGrid->addWidget(new QLabel(QStringLiteral("TX Bytes (hex)"), spiPage), 2, 0);
    spiGrid->addWidget(spi_tx_edit_, 2, 1, 1, 2);
    spiGrid->addWidget(spiSendButton, 2, 3);
    spiGrid->setColumnStretch(2, 1);

    connect(spiSendButton, &QPushButton::clicked, this, &MainWindow::onSpiWrite);

    tabs->addTab(i2cPage, QStringLiteral("ADC I2C Debug"));
    tabs->addTab(spiPage, QStringLiteral("SPI Debug"));

}

void MainWindow::rebuildSampleRowMap() {
    sample_row_map_.clear();
    if (!config_service_) {
        return;
    }

    const PowersConfig& config = config_service_->config();
    if (sample_table_mode_ == SampleTableDisplayMode::PowerId) {
        sample_row_map_.reserve(POWER_SUPPLY_COUNT);
        for (int powerId = 0; powerId < POWER_SUPPLY_COUNT; ++powerId) {
            const int rawIndex = config.supplies[powerId].means_pt;
            const int sampleId = sample_channel_map::rawToDisplay(rawIndex);
            const bool valid = sampleId >= 0 && sampleId < SAMPLE_DATA_COUNT;
            SampleTableRowMapping mapping;
            mapping.display_id = powerId;
            mapping.power_id = powerId;
            mapping.sample_id = valid ? sampleId : -1;
            mapping.data_index = valid ? sampleId : -1;
            mapping.valid = valid;
            sample_row_map_.push_back(mapping);
        }
        return;
    }

    sample_row_map_.reserve(SAMPLE_DATA_COUNT);
    for (int sampleId = 0; sampleId < SAMPLE_DATA_COUNT; ++sampleId) {
        SampleTableRowMapping mapping;
        mapping.display_id = sampleId;
        mapping.power_id = powerIdForSampleDisplayId(sampleId);
        mapping.sample_id = sampleId;
        mapping.data_index = sampleId;
        mapping.valid = true;
        sample_row_map_.push_back(mapping);
    }
}

int MainWindow::sampleIdForRow(int row) const {
    if (row < 0 || row >= static_cast<int>(sample_row_map_.size())) {
        return -1;
    }
    const SampleTableRowMapping& mapping = sample_row_map_[row];
    if (!mapping.valid || mapping.sample_id < 0 || mapping.sample_id >= SAMPLE_DATA_COUNT) {
        return -1;
    }
    return mapping.sample_id;
}

int MainWindow::dataIndexForRow(int row) const {
    if (row < 0 || row >= static_cast<int>(sample_row_map_.size())) {
        return -1;
    }
    const SampleTableRowMapping& mapping = sample_row_map_[row];
    if (!mapping.valid || mapping.data_index < 0 || mapping.data_index >= SAMPLE_DATA_COUNT) {
        return -1;
    }
    return mapping.data_index;
}

void MainWindow::appendLog(const QString& message) {
    ui->logEdit->appendPlainText(message);
    statusBar()->showMessage(message, 3000);
}

void MainWindow::waitPowerOnThread() {
    if (power_on_thread_.joinable()) {
        power_on_thread_.join();
    }
}

void MainWindow::waitCalibrationThread() {
    if (calibration_thread_.joinable()) {
        calibration_thread_.join();
    }
}

QString MainWindow::calibrationFilePath() const {
    const QString configPath = currentConfigPath();
    const QFileInfo configInfo(configPath);
    const QString dirPath = configInfo.exists()
        ? configInfo.absolutePath()
        : QFileInfo(defaultConfigPath()).absolutePath();
    return QDir(dirPath).filePath(QStringLiteral("power_calibration.json"));
}

bool MainWindow::sendDebugRequestAndWait(const DebugRequestPacket_t& request,
                                         DebugResponsePacket_t& response,
                                         int timeoutMs) {
    if (!session_service_ || !session_service_->isOpen()) {
        return false;
    }
    if (!session_service_->enterDebugSession()) {
        return false;
    }

    const bool ok = sendDebugRequestAndWaitInSession(request, response, timeoutMs);
    session_service_->exitDebugSession();
    return ok;
}

bool MainWindow::sendDebugRequestAndWaitInSession(const DebugRequestPacket_t& request,
                                                  DebugResponsePacket_t& response,
                                                  int timeoutMs) {
    if (!session_service_ || !session_service_->isOpen()) {
        return false;
    }

    QEventLoop loop(this);
    QTimer timer(this);
    timer.setSingleShot(true);

    bool received = false;
    QMetaObject::Connection responseConnection = connect(
        session_service_.get(),
        &DeviceSessionService::debugResponseReceived,
        &loop,
        [&](const DebugResponsePacket_t& packet) {
            if (packet.req_id != request.req_id) {
                return;
            }
            response = packet;
            received = true;
            loop.quit();
        });
    QMetaObject::Connection timeoutConnection = connect(&timer, &QTimer::timeout, &loop, [&]() {
        loop.quit();
    });

    const bool sent = session_service_->sendDebugRequest(request);
    if (sent) {
        timer.start(timeoutMs);
        loop.exec();
    }

    disconnect(responseConnection);
    disconnect(timeoutConnection);
    return sent && received;
}

bool MainWindow::sendI2cWriteInSession(uint8_t busId,
                                       uint8_t devAddr,
                                       uint32_t regAddr,
                                       uint8_t regLen,
                                       uint32_t value,
                                       uint8_t valueLen) {
    if (!session_service_) {
        return false;
    }
    if (regLen == 0 || regLen > 4 || valueLen == 0 || valueLen > DEBUG_REQ_MAX_DATA_LEN) {
        return false;
    }

    DebugRequestPacket_t request{};
    request.cmd_id = CMD_I2C_WRITE;
    request.req_id = session_service_->allocateDebugRequestId();
    request.bus_id = busId;
    request.target_id = devAddr;
    request.reg_len = regLen;
    request.data_len = valueLen;
    request.reg_addr = regAddr;
    for (uint8_t i = 0; i < valueLen; ++i) {
        const uint8_t shift = static_cast<uint8_t>((valueLen - 1U - i) * 8U);
        request.data[i] = static_cast<uint8_t>((value >> shift) & 0xFFU);
    }

    DebugResponsePacket_t response{};
    if (!sendDebugRequestAndWaitInSession(request, response, kDebugTimeoutMs)) {
        return false;
    }
    return response.status == DEBUG_STATUS_OK;
}

bool MainWindow::parseInputU8(QLineEdit* edit, uint8_t& value) const {
    bool ok = false;
    const QString text = edit ? edit->text().trimmed() : QString();
    const uint32_t parsed = text.toUInt(&ok, 0);
    if (!ok || parsed > 0xFFU) {
        return false;
    }
    value = static_cast<uint8_t>(parsed);
    return true;
}

bool MainWindow::parseInputU32(QLineEdit* edit, uint32_t& value) const {
    bool ok = false;
    const QString text = edit ? edit->text().trimmed() : QString();
    const qulonglong parsed = text.toULongLong(&ok, 0);
    if (!ok || parsed > 0xFFFFFFFFULL) {
        return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

bool MainWindow::parseHexBytes(const QString& text, uint8_t* out, uint8_t& outLen) const {
    if (!out) {
        return false;
    }

    QString cleaned = text.trimmed();
    if (cleaned.isEmpty()) {
        return false;
    }

    std::array<uint8_t, DEBUG_REQ_MAX_DATA_LEN> bytes{};
    uint8_t count = 0;

    const bool hasSeparator = cleaned.contains(' ') || cleaned.contains(',');
    if (hasSeparator) {
        cleaned.replace(',', ' ');
        const QStringList tokens = cleaned.split(' ', Qt::SkipEmptyParts);
        for (QString token : tokens) {
            token = token.trimmed();
            if (token.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
                token = token.mid(2);
            }
            bool ok = false;
            const uint32_t value = token.toUInt(&ok, 16);
            if (!ok || value > 0xFFU || count >= bytes.size()) {
                return false;
            }
            bytes[count++] = static_cast<uint8_t>(value);
        }
    } else {
        if (cleaned.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
            cleaned = cleaned.mid(2);
        }
        if ((cleaned.size() % 2) != 0) {
            return false;
        }
        for (int i = 0; i < cleaned.size(); i += 2) {
            if (count >= bytes.size()) {
                return false;
            }
            bool ok = false;
            const uint32_t value = cleaned.mid(i, 2).toUInt(&ok, 16);
            if (!ok || value > 0xFFU) {
                return false;
            }
            bytes[count++] = static_cast<uint8_t>(value);
        }
    }

    if (count == 0) {
        return false;
    }

    memcpy(out, bytes.data(), count);
    outLen = count;
    return true;
}

QString MainWindow::formatResponseValue(const DebugResponsePacket_t& response) const {
    if (response.data_len == 0) {
        return QStringLiteral("--");
    }

    QString hex;
    qulonglong value = 0;
    const int toCombine = std::min<int>(response.data_len, 8);
    for (int i = 0; i < response.data_len; ++i) {
        hex += QStringLiteral("%1").arg(response.data[i], 2, 16, QLatin1Char('0')).toUpper();
        if (i < toCombine) {
            value = (value << 8U) | static_cast<qulonglong>(response.data[i]);
        }
    }

    return QStringLiteral("0x%1 (%2)").arg(hex).arg(value);
}

void MainWindow::setI2cStatus(const QString& text) {
    if (i2c_status_label_) {
        i2c_status_label_->setText(text);
    }
}

void MainWindow::setSpiStatus(const QString& text) {
    if (spi_status_label_) {
        spi_status_label_->setText(text);
    }
}

void MainWindow::updateI2cValueBits(uint32_t value, uint8_t valueLen) {
    const int bitCount = qBound(0, static_cast<int>(valueLen) * 8, 16);
    for (int bit = 0; bit < static_cast<int>(i2c_value_bit_checks_.size()); ++bit) {
        QCheckBox* check = i2c_value_bit_checks_[bit];
        if (!check) {
            continue;
        }
        const bool visibleBit = bit < bitCount;
        check->setEnabled(false);
        check->setChecked(visibleBit && ((value & (1U << bit)) != 0U));
    }
}

uint16_t MainWindow::calculateIna238ShuntCal(const SampleConfig& sampleCfg) const {
    if (sampleCfg.shunt_ohm <= 0.0f || sampleCfg.max_current_a <= 0.0f) {
        return 0;
    }

    const float current_lsb = sampleCfg.max_current_a / 32768.0f;
    const float shunt_cal_float = 819.2e6f * current_lsb * sampleCfg.shunt_ohm * 4.0f;
    const float clamped = std::clamp(shunt_cal_float, 0.0f, 65535.0f);
    return static_cast<uint16_t>(clamped);
}

void MainWindow::configureIna238ShuntCalibration(const PowersConfig& config) {
    int successCount = 0;
    int failCount = 0;
    int skippedCount = 0;

    appendLog(QStringLiteral("开始下发 INA238 SHUNT_CAL ..."));
    if (!session_service_ || !session_service_->isOpen()) {
        appendLog(QStringLiteral("SHUNT_CAL 下发失败: 设备未打开"));
        return;
    }
    if (!session_service_->enterDebugSession()) {
        appendLog(QStringLiteral("SHUNT_CAL 下发失败: 无法进入 Debug 会话"));
        return;
    }

    for (int sampleId = 0; sampleId < SAMPLE_DATA_COUNT; ++sampleId) {
        const int rawIndex = sample_channel_map::displayToRaw(sampleId);
        if (rawIndex < 0 || rawIndex >= SAMPLE_DATA_COUNT) {
            ++skippedCount;
            appendLog(QStringLiteral("SHUNT_CAL 跳过: sample %1 映射无效").arg(sampleId));
            continue;
        }
        if (rawIndex == SAMPLE_DATA_COUNT - 1) {
            ++skippedCount;
            continue;
        }

        uint8_t busId = 0;
        uint8_t devAddr = 0;
        if (rawIndex < I2C1_INA238_NUM) {
            busId = DEBUG_BUS_I2C1;
            devAddr = kI2c1Ina238DevAddrs[rawIndex];
        } else {
            const int indexOnBus2 = rawIndex - I2C1_INA238_NUM;
            if (indexOnBus2 < 0 || indexOnBus2 >= I2C2_INA238_NUM) {
                ++skippedCount;
                appendLog(QStringLiteral("SHUNT_CAL 跳过: raw %1 超出 I2C2 映射范围").arg(rawIndex));
                continue;
            }
            busId = DEBUG_BUS_I2C2;
            devAddr = kI2c2Ina238DevAddrs[indexOnBus2];
        }

        const SampleConfig& sampleCfg = config.sample_cfg[sampleId];
        const uint16_t shuntCal = calculateIna238ShuntCal(sampleCfg);
        if (shuntCal == 0) {
            ++failCount;
            appendLog(QStringLiteral("SHUNT_CAL 失败: sample %1 参数无效(shunt=%2, max_current=%3)")
                          .arg(sampleId)
                          .arg(sampleCfg.shunt_ohm)
                          .arg(sampleCfg.max_current_a));
            continue;
        }

        bool ok = false;
        int attempt = 0;
        for (; attempt <= kIna238ShuntCalRetryCount; ++attempt) {
            ok = sendI2cWriteInSession(busId, devAddr, kIna238ShuntCalRegAddr, 1, shuntCal, 2);
            if (ok) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(kIna238ShuntCalWriteGapMs));
        }

        if (!ok) {
            ++failCount;
            appendLog(QStringLiteral("SHUNT_CAL 写入超时/发送失败: sample=%1 raw=%2 bus=%3 dev=0x%4")
                          .arg(sampleId)
                          .arg(rawIndex)
                          .arg(busId)
                          .arg(devAddr, 2, 16, QLatin1Char('0')));
            continue;
        }

        if (attempt > 0) {
            appendLog(QStringLiteral("SHUNT_CAL 重试成功: sample=%1 raw=%2 bus=%3 dev=0x%4")
                          .arg(sampleId)
                          .arg(rawIndex)
                          .arg(busId)
                          .arg(devAddr, 2, 16, QLatin1Char('0')));
        }

        DebugResponsePacket_t response{};
        response.status = DEBUG_STATUS_OK;
        if (response.status != DEBUG_STATUS_OK) {
            ++failCount;
            appendLog(QStringLiteral("SHUNT_CAL 写入失败: sample=%1 raw=%2 bus=%3 dev=0x%4 status=%5 err=%6")
                          .arg(sampleId)
                          .arg(rawIndex)
                          .arg(busId)
                          .arg(devAddr, 2, 16, QLatin1Char('0'))
                          .arg(static_cast<int>(response.status))
                          .arg(static_cast<int>(response.error_code)));
            continue;
        }

        ++successCount;
        std::this_thread::sleep_for(std::chrono::milliseconds(kIna238ShuntCalWriteGapMs));
    }

    session_service_->exitDebugSession();
    appendLog(QStringLiteral("SHUNT_CAL 下发完成: success=%1, failed=%2, skipped=%3")
                  .arg(successCount)
                  .arg(failCount)
                  .arg(skippedCount));
}

void MainWindow::configureInaSampleMode(const PowersConfig& config) {
    if (!config.volt_sample_en && !config.curr_sample_en) {
        appendLog(QStringLiteral("采样模式: 都不采样，跳过 INA 模式配置"));
        return;
    }
    if (!session_service_ || !session_service_->isOpen()) {
        appendLog(QStringLiteral("INA 模式配置失败: 设备未打开"));
        return;
    }
    if (!session_service_->enterDebugSession()) {
        appendLog(QStringLiteral("INA 模式配置失败: 无法进入 Debug 会话"));
        return;
    }

    const uint16_t ina238AdcConfig = config.volt_sample_en && config.curr_sample_en
        ? kIna238AdcConfigVoltageCurrent
        : (config.volt_sample_en ? kIna238AdcConfigVoltageOnly : kIna238AdcConfigCurrentOnly);
    const uint16_t ina260Config = config.volt_sample_en && config.curr_sample_en
        ? kIna260ConfigVoltageCurrent
        : (config.volt_sample_en ? kIna260ConfigVoltageOnly : kIna260ConfigCurrentOnly);

    int successCount = 0;
    int failCount = 0;
    auto writeIna238 = [&](uint8_t busId, uint8_t devAddr) {
        const bool configOk = sendI2cWriteInSession(busId, devAddr, kIna238ConfigRegAddr, 1, kIna238ConfigValue, 2);
        const bool adcOk = sendI2cWriteInSession(busId, devAddr, kIna238AdcConfigRegAddr, 1, ina238AdcConfig, 2);
        if (configOk && adcOk) {
            ++successCount;
        } else {
            ++failCount;
        }
    };

    for (uint8_t addr : kI2c1Ina238DevAddrs) {
        writeIna238(DEBUG_BUS_I2C1, addr);
    }
    for (uint8_t addr : kI2c2Ina238DevAddrs) {
        writeIna238(DEBUG_BUS_I2C2, addr);
    }

    if (sendI2cWriteInSession(DEBUG_BUS_I2C2, kIna260DevAddr, kIna260ConfigRegAddr, 1, ina260Config, 2)) {
        ++successCount;
    } else {
        ++failCount;
    }

    session_service_->exitDebugSession();
    appendLog(QStringLiteral("INA 模式配置完成: success=%1, failed=%2").arg(successCount).arg(failCount));
}

bool MainWindow::setSocPorLevel(bool high, bool updateCheckBox) {
    if (!session_service_ || !session_service_->isOpen()) {
        return false;
    }
    USBDriver* driver = session_service_->driver();
    if (!driver || !driver->isOpen()) {
        return false;
    }

    SendPinConfig(*driver, kSocPorPortGpioD, kSocPorPinPd2, high ? 1 : 0);
    if (updateCheckBox && soc_por_check_) {
        const QSignalBlocker blocker(soc_por_check_);
        soc_por_check_->setChecked(high);
    }
    appendLog(high ? QStringLiteral("SOC POR: PD2 已拉高") : QStringLiteral("SOC POR: PD2 已拉低"));
    return true;
}

void MainWindow::applyOnlineTargetVoltage(int powerId) {
    if (!power_on_completed_ || !config_service_ || !session_service_ || !session_service_->isOpen()) {
        return;
    }
    if (power_on_running_.load() || calibration_running_.load()) {
        appendLog(QStringLiteral("在线电压更新跳过: 上电/校准/采样过程中暂不下发"));
        return;
    }
    if (powerId < 0 || powerId >= POWER_SUPPLY_COUNT) {
        return;
    }

    USBDriver* driver = session_service_->driver();
    if (!driver || !driver->isOpen()) {
        return;
    }

    try {
        PowersConfig& config = config_service_->mutableConfig();
        PowerController controller(&config);
        controller.ConfigVoltage(*driver, powerId, config.supplies[powerId].tgt_volt);
        appendLog(QStringLiteral("在线更新电压: power=%1 target=%2 V")
                      .arg(powerId)
                      .arg(static_cast<double>(config.supplies[powerId].tgt_volt), 0, 'f', 3));
    } catch (const std::exception& ex) {
        appendLog(QStringLiteral("在线更新电压失败: %1").arg(QString::fromLocal8Bit(ex.what())));
    }
}

void MainWindow::refreshCalibrationButtonText() {
    if (!calibration_toggle_button_) {
        return;
    }
    calibration_toggle_button_->setText(calibration_running_.load()
        ? QStringLiteral("校准中")
        : QStringLiteral("开始校准"));
}

void MainWindow::onStartCalibrationClicked() {
    if (!config_service_ || !session_service_ || !session_service_->isOpen()) {
        appendLog(QStringLiteral("开始校准失败: 请先打开设备"));
        return;
    }
    if (power_on_running_.load()) {
        appendLog(QStringLiteral("开始校准失败: 上电流程正在执行"));
        return;
    }
    if (session_service_->isSampling()) {
        appendLog(QStringLiteral("开始校准失败: 请先停止采样"));
        return;
    }
    if (!power_on_completed_) {
        appendLog(QStringLiteral("开始校准失败: 请先完成一次上电"));
        return;
    }
    if (calibration_running_.load()) {
        appendLog(QStringLiteral("校准正在执行，请稍候"));
        return;
    }

    USBDriver* driver = session_service_->driver();
    if (!driver || !driver->isOpen()) {
        appendLog(QStringLiteral("开始校准失败: USB driver 不可用"));
        return;
    }

    waitCalibrationThread();
    setSocPorLevel(false, true);
    calibration_running_.store(true);
    updateConnectionState();
    appendLog(QStringLiteral("开始执行电压校准..."));

    PowersConfig configSnapshot = config_service_->config();
    const QString outputPath = calibrationFilePath();
    calibration_thread_ = std::thread([this, driver, configSnapshot, outputPath]() mutable {
        QString message;
        try {
            PowerController controller(&configSnapshot);
            const auto results = controller.CalibrateVoltages(*driver);
            QString error;
            if (config_service_->saveCalibrationResults(outputPath, results, &error)) {
                message = QStringLiteral("校准完成，已保存: %1").arg(QDir::toNativeSeparators(outputPath));
            } else {
                message = QStringLiteral("校准完成，但保存失败: %1").arg(error);
            }
        } catch (const std::exception& ex) {
            message = QStringLiteral("校准失败: %1").arg(QString::fromLocal8Bit(ex.what()));
        } catch (...) {
            message = QStringLiteral("校准失败: 未知异常");
        }

        QMetaObject::invokeMethod(this, [this, message]() {
            appendLog(message);
            calibration_running_.store(false);
            updateConnectionState();
        }, Qt::QueuedConnection);
    });
}

void MainWindow::onLoadCalibrationValues() {
    if (!config_service_) {
        return;
    }

    const QString filePath = calibrationFilePath();
    std::array<float, POWER_SUPPLY_COUNT> targets{};
    std::array<bool, POWER_SUPPLY_COUNT> loaded{};
    QString error;
    if (!config_service_->loadCalibrationTargets(filePath, targets, loaded, &error)) {
        appendLog(QStringLiteral("加载校准值失败: %1").arg(error));
        QMessageBox::warning(this, QStringLiteral("加载校准值失败"), error);
        return;
    }

    PowersConfig& config = config_service_->mutableConfig();
    int loadedCount = 0;
    for (int id = 0; id < POWER_SUPPLY_COUNT; ++id) {
        if (!loaded[id]) {
            continue;
        }
        config.supplies[id].tgt_volt = targets[id];
        ++loadedCount;
    }

    refreshSampleTable();
    appendLog(QStringLiteral("加载校准值完成: %1 路，%2")
                  .arg(loadedCount)
                  .arg(QDir::toNativeSeparators(filePath)));
}

void MainWindow::onRestoreDefaultValues() {
    if (!config_service_) {
        return;
    }
    if (power_on_running_.load() || calibration_running_.load()) {
        appendLog(QStringLiteral("上电或校准进行中，暂不可恢复默认参数"));
        return;
    }

    const QString filePath = currentConfigPath();
    ConfigService defaults;
    QString error;
    if (!defaults.load(filePath, &error)) {
        appendLog(QStringLiteral("恢复默认参数失败: %1").arg(error));
        QMessageBox::warning(this, QStringLiteral("恢复默认参数失败"), error);
        return;
    }

    const PowersConfig& defaultConfig = defaults.config();
    PowersConfig& config = config_service_->mutableConfig();
    for (int id = 0; id < POWER_SUPPLY_COUNT; ++id) {
        config.supplies[id].tgt_volt = defaultConfig.supplies[id].tgt_volt;
    }

    refreshSampleTable();
    appendLog(QStringLiteral("已恢复配置文件中的默认电压: %1").arg(QDir::toNativeSeparators(filePath)));
}

void MainWindow::onSocPorToggled(bool checked) {
    if (!session_service_ || !session_service_->isOpen()) {
        appendLog(QStringLiteral("SOC POR: device is not open"));
        if (soc_por_check_) {
            const QSignalBlocker blocker(soc_por_check_);
            soc_por_check_->setChecked(false);
        }
        return;
    }
    if (power_on_running_.load()) {
        appendLog(QStringLiteral("SOC POR: power-on is running"));
        if (soc_por_check_) {
            const QSignalBlocker blocker(soc_por_check_);
            soc_por_check_->setChecked(!checked);
        }
        return;
    }

    USBDriver* driver = session_service_->driver();
    if (!driver || !driver->isOpen()) {
        appendLog(QStringLiteral("SOC POR: USB driver is not available"));
        if (soc_por_check_) {
            const QSignalBlocker blocker(soc_por_check_);
            soc_por_check_->setChecked(false);
        }
        return;
    }

    setSocPorLevel(checked, false);
}

void MainWindow::onPowerOnClicked() {
    if (!session_service_ || !session_service_->isOpen()) {
        QMessageBox::information(this, QStringLiteral("设备未连接"), QStringLiteral("请先打开设备后再执行上电。"));
        return;
    }
    if (session_service_->isSampling()) {
        QMessageBox::information(this, QStringLiteral("采样进行中"), QStringLiteral("请先停止采样后再执行上电。"));
        appendLog(QStringLiteral("采样进行中，禁止执行上电"));
        return;
    }
    if (test_mode_running_.load()) {
        QMessageBox::information(this, QStringLiteral("测试模式进行中"), QStringLiteral("请先关闭测试模式后再执行上电。"));
        appendLog(QStringLiteral("测试模式进行中，禁止执行上电"));
        return;
    }
    if (calibration_running_.load()) {
        QMessageBox::information(this, QStringLiteral("校准进行中"), QStringLiteral("请等待校准完成后再执行上电。"));
        appendLog(QStringLiteral("校准进行中，禁止执行上电"));
        return;
    }
    if (!config_service_) {
        appendLog(QStringLiteral("配置服务不可用，无法执行上电"));
        return;
    }
    if (power_on_running_.load()) {
        appendLog(QStringLiteral("上电任务正在执行，请稍候"));
        return;
    }

    USBDriver* driver = session_service_->driver();
    if (!driver || !driver->isOpen()) {
        appendLog(QStringLiteral("设备驱动不可用，无法执行上电"));
        return;
    }

    if (power_on_completed_) {
        setSocPorLevel(false, true);
        SendPowerOff(*driver, &config_service_->mutableConfig());
        power_on_completed_ = false;
        if (soc_por_check_) {
            const QSignalBlocker blocker(soc_por_check_);
            soc_por_check_->setChecked(false);
        }
        appendLog(QStringLiteral("下电完成: POR/EN 已拉低"));
        updateConnectionState();
        return;
    }

    waitPowerOnThread();
    power_on_running_.store(true);
    power_on_completed_ = false;
    updateConnectionState();
    setSocPorLevel(false, true);
    appendLog(QStringLiteral("开始执行上电流程..."));

    PowersConfig configSnapshot = config_service_->config();
    configureIna238ShuntCalibration(configSnapshot);
    power_on_thread_ = std::thread([this, driver, configSnapshot]() mutable {
        QString message = QStringLiteral("上电完成");
        try {
            PowerController controller(&configSnapshot);
            controller.ConfigVoltages(*driver);
        } catch (const std::exception& ex) {
            message = QStringLiteral("上电失败: %1").arg(QString::fromLocal8Bit(ex.what()));
        } catch (...) {
            message = QStringLiteral("上电失败: 未知异常");
        }

        QMetaObject::invokeMethod(this, [this, message]() {
            appendLog(message);
            power_on_completed_ = message == QStringLiteral("上电完成");
            if (power_on_completed_) {
                QTimer::singleShot(kSocPorReleaseDelayMs, this, [this]() {
                    if (power_on_completed_ && !power_on_running_.load() && !calibration_running_.load()) {
                        setSocPorLevel(true, true);
                    }
                });
            }
            power_on_running_.store(false);
            updateConnectionState();
        }, Qt::QueuedConnection);
    });
}

void MainWindow::refreshSampleTable() {
    if (!config_service_) {
        return;
    }

    if (sample_row_map_.empty()) {
        rebuildSampleRowMap();
    }

    const PowersConfig& config = config_service_->config();
    const QSignalBlocker blocker(ui->sampleTableWidget);
    refreshing_sample_table_ = true;
    ui->sampleTableWidget->setRowCount(static_cast<int>(sample_row_map_.size()));

    for (int row = 0; row < static_cast<int>(sample_row_map_.size()); ++row) {
        const SampleTableRowMapping& mapping = sample_row_map_[row];
        const int sampleId = sampleIdForRow(row);
        const int dataIndex = dataIndexForRow(row);
        const bool validSample = sampleId >= 0;
        const bool validData = dataIndex >= 0;
        const bool validRow = validSample && validData;

        const bool editableTargetRow =
            isEditableTargetVoltageRow(config, mapping, sample_table_mode_);
        const bool hasPowerTarget = mapping.power_id >= 0 && mapping.power_id < POWER_SUPPLY_COUNT;

        QString name = QStringLiteral("N/A");
        if (sample_table_mode_ == SampleTableDisplayMode::PowerId || editableTargetRow) {
            if (validRow && hasPowerTarget) {
                const PowerSupplyConfig& supply = config.supplies[mapping.power_id];
                name = supply.name[0] == '\0'
                    ? QStringLiteral("Power %1").arg(mapping.power_id)
                    : QString::fromLocal8Bit(supply.name);
            } else if (validRow) {
                name = QStringLiteral("Power %1").arg(mapping.display_id);
            }
        } else if (validSample) {
            const SampleConfig& cfg = config.sample_cfg[sampleId];
            name = cfg.name[0] == '\0'
                ? QStringLiteral("Sample %1").arg(sampleId)
                : QString::fromLocal8Bit(cfg.name);
        }

        QTableWidgetItem* idItem = new QTableWidgetItem(QString::number(mapping.display_id));
        idItem->setTextAlignment(Qt::AlignCenter);
        ui->sampleTableWidget->setItem(row, 0, idItem);
        ui->sampleTableWidget->setItem(row, 1, new QTableWidgetItem(name));

        QTableWidgetItem* targetVoltItem = new QTableWidgetItem(QStringLiteral("--"));
        targetVoltItem->setTextAlignment(Qt::AlignCenter);
        if (validRow && editableTargetRow) {
            const PowerSupplyConfig& supply = config.supplies[mapping.power_id];
            targetVoltItem->setText(QString::number(static_cast<double>(supply.tgt_volt), 'f', 3));
            targetVoltItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        } else {
            targetVoltItem->setText(QString());
            targetVoltItem->setFlags(Qt::ItemIsSelectable);
            targetVoltItem->setBackground(QColor(224, 224, 224));
        }
        ui->sampleTableWidget->setItem(row, 2, targetVoltItem);

        QTableWidgetItem* voltEnItem = new QTableWidgetItem();
        if (validRow) {
            const SampleConfig& cfg = config.sample_cfg[sampleId];
            voltEnItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            voltEnItem->setCheckState(cfg.volt_en ? Qt::Checked : Qt::Unchecked);
        } else {
            voltEnItem->setFlags(Qt::ItemIsSelectable);
            voltEnItem->setCheckState(Qt::Unchecked);
        }
        voltEnItem->setTextAlignment(Qt::AlignCenter);
        ui->sampleTableWidget->setItem(row, 3, voltEnItem);

        QTableWidgetItem* currEnItem = new QTableWidgetItem();
        if (validRow) {
            const SampleConfig& cfg = config.sample_cfg[sampleId];
            currEnItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            currEnItem->setCheckState(cfg.current_en ? Qt::Checked : Qt::Unchecked);
        } else {
            currEnItem->setFlags(Qt::ItemIsSelectable);
            currEnItem->setCheckState(Qt::Unchecked);
        }
        currEnItem->setTextAlignment(Qt::AlignCenter);
        ui->sampleTableWidget->setItem(row, 5, currEnItem);

        const QString voltText = validData
            ? (has_sampled_volt_[dataIndex] ? formatVoltageReading(sampled_volt_[dataIndex]) : QStringLiteral("--"))
            : QStringLiteral("N/A");
        const QString currText = validData
            ? (has_sampled_curr_[dataIndex] ? formatCurrentReading(sampled_curr_[dataIndex]) : QStringLiteral("--"))
            : QStringLiteral("N/A");
        QTableWidgetItem* voltValueItem = new QTableWidgetItem(voltText);
        voltValueItem->setTextAlignment(Qt::AlignCenter);
        ui->sampleTableWidget->setItem(row, 4, voltValueItem);

        QTableWidgetItem* currValueItem = new QTableWidgetItem(currText);
        currValueItem->setTextAlignment(Qt::AlignCenter);
        ui->sampleTableWidget->setItem(row, 6, currValueItem);
    }
    refreshing_sample_table_ = false;
}

void MainWindow::onSampleTableItemChanged(QTableWidgetItem* item) {
    if (!item || refreshing_sample_table_ || !config_service_) {
        return;
    }

    const int row = item->row();
    if (row < 0 || row >= static_cast<int>(sample_row_map_.size())) {
        return;
    }

    const int sampleId = sampleIdForRow(row);
    if (sampleId < 0) {
        return;
    }

    PowersConfig& config = config_service_->mutableConfig();
    if (item->column() == 2) {
        const SampleTableRowMapping& mapping = sample_row_map_[row];
        if (!isEditableTargetVoltageRow(config, mapping, sample_table_mode_)) {
            refreshSampleTable();
            return;
        }

        bool ok = false;
        const double targetVolt = item->text().trimmed().toDouble(&ok);
        if (!ok || targetVolt < 0.0) {
            refreshSampleTable();
            return;
        }

        config.supplies[mapping.power_id].tgt_volt = static_cast<float>(targetVolt);
        const QSignalBlocker blocker(ui->sampleTableWidget);
        item->setText(QString::number(targetVolt, 'f', 3));
        applyOnlineTargetVoltage(mapping.power_id);
        refreshSampleTable();
        return;
    }

    if (item->column() == 3) {
        config.sample_cfg[sampleId].volt_en = (item->checkState() == Qt::Checked);
    } else if (item->column() == 5) {
        config.sample_cfg[sampleId].current_en = (item->checkState() == Qt::Checked);
    } else {
        return;
    }

    bool duplicateSampleId = false;
    for (int i = 0; i < static_cast<int>(sample_row_map_.size()); ++i) {
        if (i == row) {
            continue;
        }
        if (!sample_row_map_[i].valid) {
            continue;
        }
        if (sample_row_map_[i].sample_id == sampleId) {
            duplicateSampleId = true;
            break;
        }
    }
    if (duplicateSampleId) {
        refreshSampleTable();
    }
}

void MainWindow::onI2cWrite() {
    if (!session_service_ || !session_service_->isOpen()) {
        QMessageBox::warning(this, QStringLiteral("Device not open"), QStringLiteral("Please open device first."));
        setI2cStatus(QStringLiteral("Device not open"));
        return;
    }

    uint8_t slaveId = 0;
    uint32_t addr = 0;
    uint32_t value = 0;
    if (!parseInputU8(i2c_slave_id_edit_, slaveId)) {
        QMessageBox::warning(this, QStringLiteral("Input error"), QStringLiteral("Invalid slave ID (0-255)."));
        return;
    }
    if (!parseInputU32(i2c_reg_addr_edit_, addr)) {
        QMessageBox::warning(this, QStringLiteral("Input error"), QStringLiteral("Invalid register address (0-0xFFFFFFFF)."));
        return;
    }
    if (!parseInputU32(i2c_value_edit_, value)) {
        QMessageBox::warning(this, QStringLiteral("Input error"), QStringLiteral("Invalid write value (0-0xFFFFFFFF)."));
        return;
    }

    const uint8_t addrLen = static_cast<uint8_t>(i2c_addr_len_combo_->currentData().toInt());
    const uint8_t valueLen = static_cast<uint8_t>(i2c_value_len_combo_->currentData().toInt());
    const uint8_t busId = static_cast<uint8_t>(i2c_bus_combo_->currentData().toInt());

    DebugRequestPacket_t request{};
    request.cmd_id = CMD_I2C_WRITE;
    request.req_id = session_service_->allocateDebugRequestId();
    request.bus_id = busId;
    request.target_id = slaveId;
    request.reg_len = addrLen;
    request.data_len = valueLen;
    request.reg_addr = addr;
    for (uint8_t i = 0; i < valueLen; ++i) {
        const uint8_t shift = static_cast<uint8_t>((valueLen - 1U - i) * 8U);
        request.data[i] = static_cast<uint8_t>((value >> shift) & 0xFFU);
    }

    DebugResponsePacket_t response{};
    if (!sendDebugRequestAndWait(request, response, kDebugTimeoutMs)) {
        setI2cStatus(QStringLiteral("I2C write timeout or send failed"));
        return;
    }

    updateI2cValueBits(value, valueLen);
    if (response.status == DEBUG_STATUS_OK) {
        setI2cStatus(QStringLiteral("I2C write done"));
    } else {
        setI2cStatus(QStringLiteral("I2C write failed: status=%1 err=%2")
                         .arg(static_cast<int>(response.status))
                         .arg(static_cast<int>(response.error_code)));
    }
}

void MainWindow::onI2cRead() {
    if (!session_service_ || !session_service_->isOpen()) {
        QMessageBox::warning(this, QStringLiteral("Device not open"), QStringLiteral("Please open device first."));
        setI2cStatus(QStringLiteral("Device not open"));
        return;
    }

    uint8_t slaveId = 0;
    uint32_t addr = 0;
    if (!parseInputU8(i2c_slave_id_edit_, slaveId)) {
        QMessageBox::warning(this, QStringLiteral("Input error"), QStringLiteral("Invalid slave ID (0-255)."));
        return;
    }
    if (!parseInputU32(i2c_reg_addr_edit_, addr)) {
        QMessageBox::warning(this, QStringLiteral("Input error"), QStringLiteral("Invalid register address (0-0xFFFFFFFF)."));
        return;
    }

    const uint8_t addrLen = static_cast<uint8_t>(i2c_addr_len_combo_->currentData().toInt());
    const uint8_t valueLen = static_cast<uint8_t>(i2c_value_len_combo_->currentData().toInt());
    const uint8_t busId = static_cast<uint8_t>(i2c_bus_combo_->currentData().toInt());

    DebugRequestPacket_t request{};
    request.cmd_id = CMD_I2C_READ;
    request.req_id = session_service_->allocateDebugRequestId();
    request.bus_id = busId;
    request.target_id = slaveId;
    request.reg_len = addrLen;
    request.data_len = valueLen;
    request.reg_addr = addr;

    DebugResponsePacket_t response{};
    if (!sendDebugRequestAndWait(request, response, kDebugTimeoutMs)) {
        if (i2c_read_value_label_) {
            i2c_read_value_label_->setText(QStringLiteral("--"));
        }
        updateI2cValueBits(0, valueLen);
        setI2cStatus(QStringLiteral("I2C read timeout or send failed"));
        return;
    }

    if (response.status != DEBUG_STATUS_OK) {
        if (i2c_read_value_label_) {
            i2c_read_value_label_->setText(QStringLiteral("--"));
        }
        updateI2cValueBits(0, valueLen);
        setI2cStatus(QStringLiteral("I2C read failed: status=%1 err=%2")
                         .arg(static_cast<int>(response.status))
                         .arg(static_cast<int>(response.error_code)));
        return;
    }

    uint32_t value = 0;
    const int combineLen = std::min<int>(response.data_len, sizeof(value));
    for (int i = 0; i < combineLen; ++i) {
        value = (value << 8U) | response.data[i];
    }

    if (i2c_read_value_label_) {
        i2c_read_value_label_->setText(formatResponseValue(response));
    }
    updateI2cValueBits(value, response.data_len);
    setI2cStatus(QStringLiteral("I2C read done"));
}

void MainWindow::onSpiWrite() {
    if (!session_service_ || !session_service_->isOpen()) {
        QMessageBox::warning(this, QStringLiteral("Device not open"), QStringLiteral("Please open device first."));
        setSpiStatus(QStringLiteral("Device not open"));
        return;
    }

    std::array<uint8_t, DEBUG_REQ_MAX_DATA_LEN> txBytes{};
    uint8_t txLen = 0;
    if (!parseHexBytes(spi_tx_edit_ ? spi_tx_edit_->text() : QString(), txBytes.data(), txLen)) {
        QMessageBox::warning(this, QStringLiteral("Input error"), QStringLiteral("Invalid SPI TX bytes."));
        return;
    }

    DebugRequestPacket_t request{};
    request.cmd_id = CMD_SPI_WRITE;
    request.req_id = session_service_->allocateDebugRequestId();
    request.bus_id = DEBUG_BUS_SPI1;
    request.target_id = static_cast<uint8_t>(spi_cs_combo_->currentData().toInt());
    request.data_len = txLen;
    memcpy(request.data, txBytes.data(), txLen);

    DebugResponsePacket_t response{};
    if (!sendDebugRequestAndWait(request, response, kDebugTimeoutMs)) {
        setSpiStatus(QStringLiteral("SPI write timeout or send failed"));
        return;
    }

    if (response.status == DEBUG_STATUS_OK) {
        setSpiStatus(QStringLiteral("SPI write done"));
    } else {
        setSpiStatus(QStringLiteral("SPI write failed: status=%1 err=%2")
                         .arg(static_cast<int>(response.status))
                         .arg(static_cast<int>(response.error_code)));
    }
}

void MainWindow::onDataReceived(const SampleDataPacket& packet) {
    SampleDataPacketTF rawPacketTF;
    const PowersConfig* config = config_service_ ? &config_service_->config() : nullptr;
    Protocol_ParseSampleData(reinterpret_cast<const uint8_t*>(&packet), sizeof(packet), &rawPacketTF, config);
    const SampleDataPacketTF displayPacketTF = reorderPacketToDisplayOrder(rawPacketTF);

    if (waveform_recorder_) {
        waveform_recorder_->appendPacket(packet);
    }

    updateSampleValuesFromPacket(displayPacketTF);
}

void MainWindow::updateSampleValuesFromPacket(const SampleDataPacketTF& packet) {
    for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
        sampled_volt_[i] = packet.channel_volt_mv[i];
        sampled_curr_[i] = packet.channel_curr_ma[i];
        has_sampled_volt_[i] = true;
        has_sampled_curr_[i] = true;
    }

    if (waveform_widget_) {
        const PowersConfig* config = config_service_ ? &config_service_->config() : nullptr;
        waveform_widget_->updateFromPacket(packet, config);
    }

    if (sample_row_map_.empty()) {
        rebuildSampleRowMap();
    }

    const QSignalBlocker blocker(ui->sampleTableWidget);
    for (int row = 0; row < static_cast<int>(sample_row_map_.size()); ++row) {
        const int dataIndex = dataIndexForRow(row);
        if (QTableWidgetItem* voltItem = ui->sampleTableWidget->item(row, 4)) {
            voltItem->setText(dataIndex >= 0
                ? (has_sampled_volt_[dataIndex] ? formatVoltageReading(sampled_volt_[dataIndex]) : QStringLiteral("--"))
                : QStringLiteral("N/A"));
        }
        if (QTableWidgetItem* currItem = ui->sampleTableWidget->item(row, 6)) {
            currItem->setText(dataIndex >= 0
                ? (has_sampled_curr_[dataIndex] ? formatCurrentReading(sampled_curr_[dataIndex]) : QStringLiteral("--"))
                : QStringLiteral("N/A"));
        }
    }
}

void MainWindow::refreshDeviceList() {
    ui->deviceComboBox->clear();

    if (!session_service_) {
        return;
    }

    for (const USBDeviceInfo& device : session_service_->devices()) {
        const QString displayName = buildDeviceDisplayName(device);
        const QString path = QString::fromLocal8Bit(device.path.c_str());
        const QString text = QStringLiteral("%1").arg(displayName);

        ui->deviceComboBox->addItem(text, path);
    }

    if (!session_service_->devices().empty()) {
        ui->deviceComboBox->setCurrentIndex(0);
    }
}

void MainWindow::updateConnectionState() {
    const bool isOpen = session_service_ && session_service_->isOpen();
    const bool isSampling = session_service_ && session_service_->isSampling();
    const bool isPoweringOn = power_on_running_.load();
    const bool isCalibrating = calibration_running_.load();
    ui->openButton->setText(isOpen ? QStringLiteral("关闭设备") : QStringLiteral("打开设备"));
    ui->openButton->setEnabled(!isPoweringOn && !isCalibrating);
    if (sample_toggle_button_) {
        sample_toggle_button_->setEnabled(isOpen && !isPoweringOn && !isCalibrating);
        sample_toggle_button_->setText(isSampling ? QStringLiteral("停止采样") : QStringLiteral("开始采样"));
    }
    if (power_on_button_) {
        power_on_button_->setEnabled(isOpen && !isPoweringOn && !isCalibrating);
    }
    if (calibration_toggle_button_) {
        calibration_toggle_button_->setEnabled(isOpen && power_on_completed_ && !isSampling && !isPoweringOn && !isCalibrating);
    }
    refreshCalibrationButtonText();
    if (load_calibration_button_) {
        load_calibration_button_->setEnabled(!isPoweringOn && !isCalibrating);
    }
    if (restore_defaults_button_) {
        restore_defaults_button_->setEnabled(!isPoweringOn && !isCalibrating);
    }
    if (record_button_) {
        const bool isRecording = waveform_recorder_ && waveform_recorder_->isRecording();
        record_button_->setEnabled(!isCalibrating && (isRecording || isOpen || test_mode_running_.load()));
        record_button_->setText(isRecording ? QStringLiteral("停止录制") : QStringLiteral("开始录制"));
    }
    if (sample_type_combo_) {
        sample_type_combo_->setEnabled(!isPoweringOn && !isCalibrating);
    }
    if (soc_por_check_) {
        soc_por_check_->setEnabled(isOpen && !isPoweringOn && !isCalibrating);
    }
}

QString MainWindow::selectedDevicePath() const {
    if (ui->deviceComboBox->currentIndex() >= 0) {
        return ui->deviceComboBox->currentData().toString();
    }
    if (session_service_ && !session_service_->devices().empty()) {
        return QString::fromLocal8Bit(session_service_->devices().front().path.c_str());
    }
    return {};
}

void MainWindow::onQueryDevices() {
    if (!session_service_) {
        return;
    }

    session_service_->queryDevices();
    refreshDeviceList();

    appendLog(QStringLiteral("查询完成: 共 %1 个设备")
                  .arg(session_service_->devices().size()));
}

void MainWindow::onToggleOpenDevice() {
    if (!session_service_) {
        return;
    }
    if (power_on_running_.load()) {
        appendLog(QStringLiteral("上电进行中，暂不可打开或关闭设备"));
        return;
    }
    if (calibration_running_.load()) {
        appendLog(QStringLiteral("校准进行中，暂不可打开或关闭设备"));
        return;
    }

    if (session_service_->isOpen()) {
        if (session_service_->isSampling()) {
            const PowersConfig& config = config_service_->config();
            session_service_->stopSampling(config);
        }
        session_service_->closeDevice();
        power_on_completed_ = false;
        appendLog(QStringLiteral("设备已关闭"));
        updateConnectionState();
        return;
    }

    if (session_service_->devices().empty()) {
        onQueryDevices();
        if (session_service_->devices().empty()) {
            QMessageBox::information(this, QStringLiteral("未发现设备"), QStringLiteral("当前默认配置下没有可打开的设备。"));
            return;
        }
    }

    const QString path = selectedDevicePath();
    if (path.isEmpty()) {
        appendLog(QStringLiteral("未找到可用设备路径"));
        return;
    }

    if (!session_service_->openDevice(path)) {
        appendLog(QStringLiteral("打开设备失败: %1").arg(path));
        power_on_completed_ = false;
        updateConnectionState();
        return;
    }

    power_on_completed_ = false;
    appendLog(QStringLiteral("设备已打开: %1").arg(path));
    updateConnectionState();
}

void MainWindow::onToggleSampling() {
    if (!session_service_ || !session_service_->isOpen()) {
        QMessageBox::information(this, QStringLiteral("未连接设备"), QStringLiteral("请先打开设备后再开始采样。"));
        return;
    }

    if (power_on_running_.load()) {
        QMessageBox::information(this, QStringLiteral("上电进行中"), QStringLiteral("请等待上电完成后再操作采样。"));
        appendLog(QStringLiteral("上电进行中，暂不可切换采样"));
        return;
    }
    if (calibration_running_.load()) {
        QMessageBox::information(this, QStringLiteral("校准进行中"), QStringLiteral("请等待校准完成后再操作采样。"));
        appendLog(QStringLiteral("校准进行中，暂不可切换采样"));
        return;
    }

    if (!session_service_->isSampling()) {
        if (test_mode_running_.load()) {
            if (test_mode_action_) {
                const QSignalBlocker blocker(test_mode_action_);
                test_mode_action_->setChecked(false);
            }
            stopTestModeThread();
        }
        if (waveform_widget_) {
            waveform_widget_->clearSamples();
        }
        const PowersConfig& config = config_service_->config();
        if (!config.volt_sample_en && !config.curr_sample_en) {
            appendLog(QStringLiteral("采样模式为都不采样，未启动采样"));
            updateConnectionState();
            return;
        }
        configureInaSampleMode(config);
        configureIna238ShuntCalibration(config);
        session_service_->startSampling(config);
        appendLog(QStringLiteral("开始采样"));
    } else {
        const PowersConfig& config = config_service_->config();
        session_service_->stopSampling(config);
        appendLog(QStringLiteral("停止采样"));
    }

    updateConnectionState();
}

void MainWindow::onToggleTestMode(bool enabled) {
    if (enabled) {
        startTestModeThread();
        return;
    }
    stopTestModeThread();
}

void MainWindow::startTestModeThread() {
    if (test_mode_running_.load()) {
        return;
    }
    if (power_on_running_.load()) {
        if (test_mode_action_) {
            const QSignalBlocker blocker(test_mode_action_);
            test_mode_action_->setChecked(false);
        }
        appendLog(QStringLiteral("上电进行中，无法启动测试模式"));
        return;
    }
    if (calibration_running_.load()) {
        if (test_mode_action_) {
            const QSignalBlocker blocker(test_mode_action_);
            test_mode_action_->setChecked(false);
        }
        appendLog(QStringLiteral("校准进行中，无法启动测试模式"));
        return;
    }

    if (session_service_ && session_service_->isSampling()) {
        if (test_mode_action_) {
            const QSignalBlocker blocker(test_mode_action_);
            test_mode_action_->setChecked(false);
        }
        appendLog(QStringLiteral("设备采样中，无法启动测试模式"));
        return;
    }

    test_mode_running_.store(true);
    appendLog(QStringLiteral("测试模式已启动"));
    updateConnectionState();

    test_mode_thread_ = std::thread([this]() {
        using namespace std::chrono_literals;
        uint32_t tick = 0;

        while (test_mode_running_.load()) {
            SampleDataPacket packet{};
            packet.report_id = 0;
            packet.timestamp = tick * 80;

            for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
                const float basePhase = static_cast<float>(tick) * 0.16f + static_cast<float>(i) * 0.22f;
                const float volt = 12000.0f + 1200.0f * std::sin(basePhase);
                const float curr = 850.0f + 260.0f * std::sin(basePhase + 1.1f);
                const float voltReg = volt / (INA238_VBUS_LSB_V * 1000.0f);
                packet.channel_volt_reg[i] = static_cast<uint16_t>(std::clamp(voltReg, 0.0f, 65535.0f));
                packet.channel_curr_reg[i] = static_cast<uint16_t>(std::max(0.0f, curr));
            }

            QMetaObject::invokeMethod(this, [this, packet]() {
                onDataReceived(packet);
            }, Qt::QueuedConnection);

            std::this_thread::sleep_for(40ms);
            ++tick;
        }
    });
}

void MainWindow::stopTestModeThread() {
    const bool wasRunning = test_mode_running_.exchange(false);
    if (test_mode_thread_.joinable()) {
        test_mode_thread_.join();
    }
    if (wasRunning) {
        appendLog(QStringLiteral("测试模式已停止"));
    }
    updateConnectionState();
}

void MainWindow::onToggleRecording() {
    if (!waveform_recorder_) {
        return;
    }
    if (calibration_running_.load()) {
        appendLog(QStringLiteral("校准进行中，无法切换录制"));
        return;
    }

    if (!waveform_recorder_->isRecording()) {
        if (!(test_mode_running_.load() || (session_service_ && session_service_->isOpen()))) {
            QMessageBox::information(this, QStringLiteral("无法录制"), QStringLiteral("请先打开设备或启动测试模式。"));
            return;
        }
        const QString defaultDirectory = currentConfigPath().isEmpty()
            ? QCoreApplication::applicationDirPath()
            : QFileInfo(QDir::fromNativeSeparators(currentConfigPath())).absolutePath();
        waveform_recorder_->start(defaultDirectory);
        appendLog(QStringLiteral("波形录制已开始"));
        updateConnectionState();
        return;
    }

    stopRecording(true);
}

void MainWindow::stopRecording(bool exportFile) {
    if (!waveform_recorder_ || !waveform_recorder_->isRecording()) {
        return;
    }

    waveform_recorder_->stop();
    updateConnectionState();

    if (!exportFile) {
        waveform_recorder_->clear();
        return;
    }

    if (!waveform_recorder_->hasData()) {
        appendLog(QStringLiteral("录制结束，但没有采样数据可导出"));
        QMessageBox::information(this, QStringLiteral("录制完成"), QStringLiteral("本次录制没有采样数据。"));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存波形录制"),
        waveform_recorder_->defaultFilePath(),
        QStringLiteral("CSV Files (*.csv);;All Files (*.*)"));

    if (filePath.isEmpty()) {
        appendLog(QStringLiteral("已取消保存波形录制"));
        waveform_recorder_->clear();
        return;
    }

    const PowersConfig* config = config_service_ ? &config_service_->config() : nullptr;
    if (waveform_recorder_->exportCsv(filePath, config)) {
        QMessageBox::information(this, QStringLiteral("保存成功"), QStringLiteral("波形录制已保存到: %1").arg(QDir::toNativeSeparators(filePath)));
        appendLog(QStringLiteral("波形录制保存成功: %1").arg(QDir::toNativeSeparators(filePath)));
        appendLog(QStringLiteral("波形录制已保存"));
    } else {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("无法写入文件: %1").arg(QDir::toNativeSeparators(filePath)));
        appendLog(QStringLiteral("波形录制保存失败: %1").arg(QDir::toNativeSeparators(filePath)));
    }
    waveform_recorder_->clear();
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

    QString errorMessage;
    if (!config_service_ || !config_service_->load(filePath, &errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("配置加载失败"), errorMessage.isEmpty() ? QStringLiteral("配置加载失败") : errorMessage);
        appendLog(QStringLiteral("配置加载失败: %1").arg(QDir::toNativeSeparators(filePath)));
        return;
    }

    rebuildSampleRowMap();
    refreshSampleTable();
    refreshCalibrationButtonText();
    refreshSampleTypeSelector();
    addConfigPathOption(filePath);
    appendLog(QStringLiteral("配置加载成功: %1").arg(QDir::toNativeSeparators(filePath)));
}
