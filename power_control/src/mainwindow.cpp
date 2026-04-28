#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QBoxLayout>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QMetaObject>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionButton>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <thread>

#include "config_service.h"
#include "device_session_service.h"
#include "debug_window.h"
#include "adc_def.h"
#include "power_control.h"
#include "sample_channel_map.h"
#include "stm32_comm.h"
#include "waveform_recorder.h"
#include "waveform_widget.h"

namespace {
constexpr uint16_t kDefaultVid = 0x0483;
constexpr uint16_t kDefaultPid = 0x5750;

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
    resize(width(), 760);

    QMenu* toolsMenu = menuBar()->addMenu(QStringLiteral("Tools"));
    QAction* debugInterfaceAction = toolsMenu->addAction(QStringLiteral("Debug Interface"));
    connect(debugInterfaceAction, &QAction::triggered, this, &MainWindow::onOpenDebugInterface);
    test_mode_action_ = toolsMenu->addAction(QStringLiteral("测试模式"));
    test_mode_action_->setCheckable(true);
    connect(test_mode_action_, &QAction::toggled, this, &MainWindow::onToggleTestMode);
    power_on_button_ = new QPushButton(QStringLiteral("上电"), this);
    ui->statusLayout->insertWidget(3, power_on_button_);
    connect(power_on_button_, &QPushButton::clicked, this, &MainWindow::onPowerOnClicked);
    calibration_toggle_button_ = new QPushButton(this);
    ui->statusLayout->insertWidget(4, calibration_toggle_button_);
    connect(calibration_toggle_button_, &QPushButton::clicked, this, &MainWindow::onToggleCalibrationClicked);
    record_button_ = new QPushButton(QStringLiteral("开始录制"), this);
    record_button_->setEnabled(false);
    ui->statusLayout->insertWidget(5, record_button_);
    connect(record_button_, &QPushButton::clicked, this, &MainWindow::onToggleRecording);

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
    power_on_button_->setMinimumWidth(buttonWidth);
    power_on_button_->setMinimumHeight(buttonHeight);
    calibration_toggle_button_->setMinimumWidth(buttonWidth);
    calibration_toggle_button_->setMinimumHeight(buttonHeight);
    refreshCalibrationButtonText();

    const int leftLabelWidth = qMax(ui->configPathLabel->sizeHint().width(), ui->deviceNameLabel->sizeHint().width());
    ui->configPathLabel->setMinimumWidth(leftLabelWidth);
    ui->deviceNameLabel->setMinimumWidth(leftLabelWidth);

    ui->configLayout->setStretch(0, 0);
    ui->configLayout->setStretch(1, 1);
    ui->configLayout->setStretch(2, 0);
    ui->configLayout->setStretch(3, 0);

    ui->controlLayout->setStretch(0, 0);
    ui->controlLayout->setStretch(1, 1);
    ui->controlLayout->setStretch(2, 0);
    ui->controlLayout->setStretch(3, 0);

    auto* waveformLayout = new QVBoxLayout(ui->waveformHost);
    waveformLayout->setContentsMargins(0, 0, 0, 0);
    waveform_widget_ = new WaveformWidget(ui->waveformHost);
    waveformLayout->addWidget(waveform_widget_);

    if (QBoxLayout* mainLayout = qobject_cast<QBoxLayout*>(ui->centralwidget->layout())) {
        mainLayout->setStretch(0, 0);
        mainLayout->setStretch(1, 1);
        mainLayout->setStretch(2, 0);
    }

    ui->sampleGroupBox->setMinimumHeight(420);
    ui->sampleTableWidget->setMinimumHeight(320);

    QHeaderView* sampleHeader = ui->sampleTableWidget->horizontalHeader();
    sampleHeader->setStretchLastSection(false);
    sampleHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
    sampleHeader->setSectionResizeMode(1, QHeaderView::Stretch);
    sampleHeader->setSectionResizeMode(5, QHeaderView::Fixed);
    ui->sampleTableWidget->setColumnWidth(5, 78);
    ui->sampleTableWidget->verticalHeader()->setVisible(false);
    ui->sampleTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->sampleTableWidget->verticalHeader()->setDefaultSectionSize(20);
    ui->sampleTableWidget->verticalHeader()->setMinimumSectionSize(20);
    ui->sampleTableWidget->setAlternatingRowColors(true);
    ui->sampleTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->sampleTableWidget->setItemDelegateForColumn(2, new CenteredCheckBoxDelegate(ui->sampleTableWidget));
    ui->sampleTableWidget->setItemDelegateForColumn(4, new CenteredCheckBoxDelegate(ui->sampleTableWidget));
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
    for (int column : {0, 2, 3, 4, 5}) {
        if (QTableWidgetItem* headerItem = ui->sampleTableWidget->horizontalHeaderItem(column)) {
            headerItem->setTextAlignment(Qt::AlignCenter);
        }
    }
    setupSampleModeSelector();

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
    stopRecording(false);
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
    QVBoxLayout* sampleLayout = qobject_cast<QVBoxLayout*>(ui->sampleGroupBox->layout());
    if (!sampleLayout) {
        return;
    }

    auto* modeLayout = new QHBoxLayout();
    auto* modeLabel = new QLabel(QStringLiteral("显示模式"), ui->sampleGroupBox);
    sample_mode_combo_ = new QComboBox(ui->sampleGroupBox);
    sample_mode_combo_->addItem(QStringLiteral("按电源ID"), static_cast<int>(SampleTableDisplayMode::PowerId));
    sample_mode_combo_->addItem(QStringLiteral("按采样通道"), static_cast<int>(SampleTableDisplayMode::SampleChannel));
    sample_mode_combo_->setCurrentIndex(0);

    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(sample_mode_combo_);
    modeLayout->addStretch(1);
    sampleLayout->insertLayout(0, modeLayout);

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
        mapping.power_id = -1;
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

void MainWindow::refreshCalibrationButtonText() {
    if (!calibration_toggle_button_) {
        return;
    }
    const bool calibrationEnabled = config_service_ && config_service_->config().calibration_en;
    calibration_toggle_button_->setText(calibrationEnabled ? QStringLiteral("校准: 开") : QStringLiteral("校准: 关"));
}

void MainWindow::onToggleCalibrationClicked() {
    if (!config_service_) {
        return;
    }
    if (power_on_running_.load()) {
        appendLog(QStringLiteral("上电进行中，暂不可切换校准状态"));
        return;
    }
    PowersConfig& config = config_service_->mutableConfig();
    config.calibration_en = !config.calibration_en;
    refreshCalibrationButtonText();
    appendLog(config.calibration_en ? QStringLiteral("校准已开启") : QStringLiteral("校准已关闭"));
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

    waitPowerOnThread();
    power_on_running_.store(true);
    updateConnectionState();
    appendLog(QStringLiteral("开始执行上电流程..."));

    PowersConfig configSnapshot = config_service_->config();
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

        QString name = QStringLiteral("N/A");
        if (sample_table_mode_ == SampleTableDisplayMode::PowerId) {
            if (validRow && mapping.power_id >= 0 && mapping.power_id < POWER_SUPPLY_COUNT) {
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
        ui->sampleTableWidget->setItem(row, 2, voltEnItem);

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
        ui->sampleTableWidget->setItem(row, 4, currEnItem);

        const QString voltText = validData
            ? (has_sampled_volt_[dataIndex] ? QString::number(sampled_volt_[dataIndex]) : QStringLiteral("--"))
            : QStringLiteral("N/A");
        const QString currText = validData
            ? (has_sampled_curr_[dataIndex] ? QString::number(sampled_curr_[dataIndex]) : QStringLiteral("--"))
            : QStringLiteral("N/A");
        QTableWidgetItem* voltValueItem = new QTableWidgetItem(voltText);
        voltValueItem->setTextAlignment(Qt::AlignCenter);
        ui->sampleTableWidget->setItem(row, 3, voltValueItem);

        QTableWidgetItem* currValueItem = new QTableWidgetItem(currText);
        currValueItem->setTextAlignment(Qt::AlignCenter);
        ui->sampleTableWidget->setItem(row, 5, currValueItem);
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
        config.sample_cfg[sampleId].volt_en = (item->checkState() == Qt::Checked);
    } else if (item->column() == 4) {
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

void MainWindow::onDataReceived(const SampleDataPacket& packet) {
    SampleDataPacketTF rawPacketTF;
    Protocol_ParseSampleData(reinterpret_cast<const uint8_t*>(&packet), sizeof(packet), &rawPacketTF);
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
        if (QTableWidgetItem* voltItem = ui->sampleTableWidget->item(row, 3)) {
            voltItem->setText(dataIndex >= 0
                ? (has_sampled_volt_[dataIndex] ? QString::number(sampled_volt_[dataIndex]) : QStringLiteral("--"))
                : QStringLiteral("N/A"));
        }
        if (QTableWidgetItem* currItem = ui->sampleTableWidget->item(row, 5)) {
            currItem->setText(dataIndex >= 0
                ? (has_sampled_curr_[dataIndex] ? QString::number(sampled_curr_[dataIndex]) : QStringLiteral("--"))
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
    ui->openButton->setText(isOpen ? QStringLiteral("关闭设备") : QStringLiteral("打开设备"));
    ui->statusValueLabel->setText(isOpen ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    ui->openButton->setEnabled(!isPoweringOn);
    ui->sampleToggleButton->setEnabled(isOpen && !isPoweringOn);
    ui->sampleToggleButton->setText(isSampling ? QStringLiteral("停止采样") : QStringLiteral("开始采样"));
    if (power_on_button_) {
        power_on_button_->setEnabled(isOpen && !isPoweringOn);
    }
    if (calibration_toggle_button_) {
        calibration_toggle_button_->setEnabled(!isPoweringOn);
    }
    refreshCalibrationButtonText();
    if (record_button_) {
        const bool isRecording = waveform_recorder_ && waveform_recorder_->isRecording();
        record_button_->setEnabled(isRecording || isOpen || test_mode_running_.load());
        record_button_->setText(isRecording ? QStringLiteral("停止录制") : QStringLiteral("开始录制"));
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

    if (session_service_->isOpen()) {
        if (session_service_->isSampling()) {
            const PowersConfig& config = config_service_->config();
            session_service_->stopSampling(config);
        }
        session_service_->closeDevice();
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
        updateConnectionState();
        return;
    }

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

    if (waveform_recorder_->exportCsv(filePath)) {
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
    addConfigPathOption(filePath);
    appendLog(QStringLiteral("配置加载成功: %1").arg(QDir::toNativeSeparators(filePath)));
}

void MainWindow::onOpenDebugInterface() {
    auto* debugWindow = new DebugInterfaceWindow(session_service_.get());

    debug_windows_.push_back(debugWindow);
    connect(debugWindow, &QObject::destroyed, this, [this, debugWindow]() {
        const auto it = std::remove(debug_windows_.begin(), debug_windows_.end(), debugWindow);
        debug_windows_.erase(it, debug_windows_.end());
    });

    debugWindow->show();
    debugWindow->raise();
    debugWindow->activateWindow();
}
