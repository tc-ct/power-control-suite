#include "debug_window.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QString>

#include <array>
#include <cstring>

#include "proto_pkg.h"
#include "usb_driver.h"

namespace {
#pragma pack(push, 1)
struct I2CDebugWritePacket {
    uint8_t cmd_id;
    uint8_t slave_id;
    uint32_t addr;
    uint8_t addr_len;
    uint32_t value;
    uint8_t reserved[53];
};

struct I2CDebugReadPacket {
    uint8_t cmd_id;
    uint8_t slave_id;
    uint32_t addr;
    uint8_t addr_len;
    uint8_t reserved[57];
};
#pragma pack(pop)

static_assert(sizeof(I2CDebugWritePacket) == DOWNLOAD_PACKET_SIZE, "I2C write packet must be 64 bytes");
static_assert(sizeof(I2CDebugReadPacket) == DOWNLOAD_PACKET_SIZE, "I2C read packet must be 64 bytes");
}

DebugInterfaceWindow::DebugInterfaceWindow(std::function<USBDriver*()> usbProvider, QWidget* parent)
    : QMainWindow(parent), usb_provider_(std::move(usbProvider))
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle(QStringLiteral("Debug Interface"));
    resize(520, 320);
    buildUi();
}

void DebugInterfaceWindow::buildUi() {
    auto* tabs = new QTabWidget(this);
    setCentralWidget(tabs);

    auto* i2cPage = new QWidget(tabs);
    auto* i2cLayout = new QHBoxLayout(i2cPage);
    i2cLayout->setContentsMargins(6, 6, 6, 6);
    i2cLayout->setSpacing(6);

    auto* i2cGroup = new QGroupBox(QStringLiteral("I2C"), i2cPage);
    auto* i2cGrid = new QGridLayout(i2cGroup);
    i2cGrid->setContentsMargins(6, 6, 6, 6);
    i2cGrid->setHorizontalSpacing(6);
    i2cGrid->setVerticalSpacing(4);

    i2c_slave_id_edit_ = new QLineEdit(i2cGroup);
    i2c_slave_id_edit_->setPlaceholderText(QStringLiteral("0x22"));
    i2c_slave_id_edit_->setText(QStringLiteral("0x22"));

    i2c_reg_addr_edit_ = new QLineEdit(i2cGroup);
    i2c_reg_addr_edit_->setPlaceholderText(QStringLiteral("0x310040f8"));
    i2c_reg_addr_edit_->setText(QStringLiteral("0x310040f8"));

    i2c_addr_len_combo_ = new QComboBox(i2cGroup);
    for (int i = 1; i <= 8; ++i) {
        i2c_addr_len_combo_->addItem(QString::number(i), i);
    }
    i2c_addr_len_combo_->setCurrentIndex(1);

    i2c_value_len_combo_ = new QComboBox(i2cGroup);
    for (int i = 1; i <= 8; ++i) {
        i2c_value_len_combo_->addItem(QString::number(i), i);
    }

    i2c_value_edit_ = new QLineEdit(i2cGroup);
    i2c_value_edit_->setPlaceholderText(QStringLiteral("0x00"));

    auto* writeButton = new QPushButton(QStringLiteral("Write"), i2cGroup);
    auto* readButton = new QPushButton(QStringLiteral("Read"), i2cGroup);

    i2c_value_label_ = new QLabel(QStringLiteral("--"), i2cGroup);
    i2c_status_label_ = new QLabel(QStringLiteral("Idle"), i2cGroup);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Status"), i2cGroup), 0, 0);
    i2cGrid->addWidget(i2c_status_label_, 0, 1, 1, 3);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Slave Addr(7bit)"), i2cGroup), 1, 0);
    i2cGrid->addWidget(i2c_slave_id_edit_, 1, 1, 1, 3);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Addr Length"), i2cGroup), 2, 0);
    i2cGrid->addWidget(i2c_addr_len_combo_, 2, 1, 1, 3);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Reg Addr"), i2cGroup), 3, 0);
    i2cGrid->addWidget(i2c_reg_addr_edit_, 3, 1, 1, 3);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Value Length"), i2cGroup), 4, 0);
    i2cGrid->addWidget(i2c_value_len_combo_, 4, 1, 1, 3);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Value"), i2cGroup), 5, 0);
    i2cGrid->addWidget(i2c_value_edit_, 5, 1, 1, 3);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Read Value"), i2cGroup), 6, 0);
    i2cGrid->addWidget(i2c_value_label_, 6, 1, 1, 3);

    i2cGrid->addWidget(readButton, 7, 0);
    i2cGrid->addWidget(writeButton, 7, 1);

    auto* listGroup = new QGroupBox(QStringLiteral("Read List"), i2cPage);
    auto* listLayout = new QVBoxLayout(listGroup);
    listLayout->setContentsMargins(6, 6, 6, 6);
    listLayout->setSpacing(4);
    auto* rangeLayout = new QHBoxLayout();
    rangeLayout->setSpacing(4);
    i2c_list_from_edit_ = new QLineEdit(listGroup);
    i2c_list_from_edit_->setPlaceholderText(QStringLiteral("0x3100d420"));
    i2c_list_from_edit_->setText(QStringLiteral("0x3100d420"));
    i2c_list_to_edit_ = new QLineEdit(listGroup);
    i2c_list_to_edit_->setPlaceholderText(QStringLiteral("0x3100d430"));
    i2c_list_to_edit_->setText(QStringLiteral("0x3100d430"));
    auto* readListButton = new QPushButton(QStringLiteral("Read List"), listGroup);

    rangeLayout->addWidget(i2c_list_from_edit_);
    rangeLayout->addWidget(new QLabel(QStringLiteral("~"), listGroup));
    rangeLayout->addWidget(i2c_list_to_edit_);
    rangeLayout->addWidget(readListButton);

    i2c_list_table_ = new QTableWidget(listGroup);
    i2c_list_table_->setColumnCount(4);
    i2c_list_table_->setHorizontalHeaderLabels({
        QStringLiteral("Addr"),
        QStringLiteral("Addr Len"),
        QStringLiteral("Value Len"),
        QStringLiteral("Value")});
    i2c_list_table_->verticalHeader()->setVisible(false);
    i2c_list_table_->horizontalHeader()->setStretchLastSection(true);
    i2c_list_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    i2c_list_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    i2c_list_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    i2c_list_table_->setShowGrid(true);
    i2c_list_table_->setAlternatingRowColors(false);
    i2c_list_table_->setStyleSheet(
        "QTableWidget::item { padding-top: 1px; padding-bottom: 1px; }"
        "QHeaderView::section { padding-top: 1px; padding-bottom: 1px; }");
    i2c_list_table_->verticalHeader()->setDefaultSectionSize(18);
    i2c_list_table_->verticalHeader()->setMinimumSectionSize(16);
    i2c_list_table_->horizontalHeader()->setFixedHeight(20);

    listLayout->addLayout(rangeLayout);
    listLayout->addWidget(i2c_list_table_);

    i2cLayout->addWidget(i2cGroup, 1);
    i2cLayout->addWidget(listGroup, 1);

    connect(writeButton, &QPushButton::clicked, this, &DebugInterfaceWindow::onI2cWrite);
    connect(readButton, &QPushButton::clicked, this, &DebugInterfaceWindow::onI2cRead);
    connect(readListButton, &QPushButton::clicked, this, &DebugInterfaceWindow::onI2cReadList);

    auto* spiPage = new QWidget(tabs);
    auto* spiLayout = new QVBoxLayout(spiPage);
    spiLayout->addWidget(new QLabel(QStringLiteral("SPI debug page (to be expanded)."), spiPage));
    spiLayout->addStretch(1);

    tabs->addTab(i2cPage, QStringLiteral("i2c debug"));
    tabs->addTab(spiPage, QStringLiteral("spi debug"));
}

bool DebugInterfaceWindow::parseInputU8(QLineEdit* edit, uint8_t& value) const {
    bool ok = false;
    const QString text = edit->text().trimmed();
    uint32_t parsed = text.toUInt(&ok, 0);
    if (!ok || parsed > 0xFFU) {
        return false;
    }
    value = static_cast<uint8_t>(parsed);
    return true;
}

bool DebugInterfaceWindow::parseInputU32(QLineEdit* edit, uint32_t& value) const {
    bool ok = false;
    const QString text = edit->text().trimmed();
    const qulonglong parsed = text.toULongLong(&ok, 0);
    if (!ok || parsed > 0xFFFFFFFFULL) {
        return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

void DebugInterfaceWindow::setI2cStatus(const QString& text) {
    if (i2c_status_label_) {
        i2c_status_label_->setText(text);
    }
}

void DebugInterfaceWindow::onI2cWrite() {
    USBDriver* dev = usb_provider_ ? usb_provider_() : nullptr;
    if (!dev || !dev->isOpen()) {
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

    I2CDebugWritePacket pkt{};
    pkt.cmd_id = CMD_I2C_WRITE;
    pkt.slave_id = slaveId;
    pkt.addr = addr;
    pkt.addr_len = static_cast<uint8_t>(i2c_addr_len_combo_->currentData().toInt());
    pkt.value = value;

    if (!dev->send(reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt))) {
        setI2cStatus(QStringLiteral("I2C write failed"));
        return;
    }

    setI2cStatus(QStringLiteral("I2C write sent"));
}

void DebugInterfaceWindow::onI2cRead() {
    USBDriver* dev = usb_provider_ ? usb_provider_() : nullptr;
    if (!dev || !dev->isOpen()) {
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
    uint32_t value = 0;
    if (!sendI2cReadRequest(slaveId, addr, addrLen, value)) {
        i2c_value_label_->setText(QStringLiteral("--"));
        return;
    }

    i2c_value_label_->setText(QStringLiteral("0x%1 (%2)")
                                  .arg(value, 8, 16, QLatin1Char('0'))
                                  .arg(value));
    setI2cStatus(QStringLiteral("I2C read done"));
}

bool DebugInterfaceWindow::sendI2cReadRequest(uint8_t slaveId, uint32_t addr, uint8_t addrLen, uint32_t& outValue) {
    USBDriver* dev = usb_provider_ ? usb_provider_() : nullptr;
    if (!dev || !dev->isOpen()) {
        QMessageBox::warning(this, QStringLiteral("Device not open"), QStringLiteral("Please open device first."));
        setI2cStatus(QStringLiteral("Device not open"));
        return false;
    }

    I2CDebugReadPacket pkt{};
    pkt.cmd_id = CMD_I2C_READ;
    pkt.slave_id = slaveId;
    pkt.addr = addr;
    pkt.addr_len = addrLen;

    if (!dev->send(reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt))) {
        setI2cStatus(QStringLiteral("I2C read request failed"));
        return false;
    }

    std::array<uint8_t, USB_REPORT_SIZE> rx{};
    const int bytes = dev->receive(rx.data(), rx.size());
    if (bytes <= 0) {
        setI2cStatus(QStringLiteral("I2C read sent (no response yet)"));
        return false;
    }

    if (bytes >= static_cast<int>(sizeof(uint32_t))) {
        memcpy(&outValue, rx.data(), sizeof(uint32_t));
    } else {
        outValue = rx[0];
    }
    return true;
}

void DebugInterfaceWindow::onI2cReadList() {
    USBDriver* dev = usb_provider_ ? usb_provider_() : nullptr;
    if (!dev || !dev->isOpen()) {
        QMessageBox::warning(this, QStringLiteral("Device not open"), QStringLiteral("Please open device first."));
        setI2cStatus(QStringLiteral("Device not open"));
        return;
    }
    uint8_t slaveId = 0;
    uint32_t fromAddr = 0;
    uint32_t toAddr = 0;

    if (!parseInputU8(i2c_slave_id_edit_, slaveId)) {
        QMessageBox::warning(this, QStringLiteral("Input error"), QStringLiteral("Invalid slave ID (0-255)."));
        return;
    }
    if (!parseInputU32(i2c_list_from_edit_, fromAddr) || !parseInputU32(i2c_list_to_edit_, toAddr)) {
        QMessageBox::warning(this, QStringLiteral("Input error"), QStringLiteral("Invalid range address (0-0xFFFFFFFF)."));
        return;
    }

    if (fromAddr > toAddr) {
        QMessageBox::warning(this, QStringLiteral("Input error"), QStringLiteral("Range start must be <= end."));
        return;
    }

    const uint64_t count = static_cast<uint64_t>(toAddr) - static_cast<uint64_t>(fromAddr) + 1ULL;
    if (count > 256ULL) {
        QMessageBox::warning(this, QStringLiteral("Input error"), QStringLiteral("Read list range too large (max 256)."));
        return;
    }

    const uint8_t addrLen = static_cast<uint8_t>(i2c_addr_len_combo_->currentData().toInt());
    const uint8_t valueLen = static_cast<uint8_t>(i2c_value_len_combo_->currentData().toInt());
    i2c_list_table_->setRowCount(static_cast<int>(count));

    for (uint64_t i = 0; i < count; ++i) {
        const uint32_t addr = static_cast<uint32_t>(fromAddr + i);
        uint32_t value = 0;
        const bool ok = sendI2cReadRequest(slaveId, addr, addrLen, value);

        auto* addrItem = new QTableWidgetItem(QStringLiteral("0x%1").arg(addr, 8, 16, QLatin1Char('0')));
        QTableWidgetItem* valueItem = nullptr;
        if (ok) {
            valueItem = new QTableWidgetItem(QStringLiteral("0x%1 (%2)")
                                                 .arg(value, 8, 16, QLatin1Char('0'))
                                                 .arg(value));
        } else {
            valueItem = new QTableWidgetItem(QStringLiteral("--"));
        }
        auto* addrLenItem = new QTableWidgetItem(QString::number(addrLen));
        auto* valueLenItem = new QTableWidgetItem(QString::number(valueLen));
        const int row = static_cast<int>(i);
        i2c_list_table_->setItem(row, 0, addrItem);
        i2c_list_table_->setItem(row, 1, addrLenItem);
        i2c_list_table_->setItem(row, 2, valueLenItem);
        i2c_list_table_->setItem(row, 3, valueItem);
    }

    setI2cStatus(QStringLiteral("I2C read list done"));
}
