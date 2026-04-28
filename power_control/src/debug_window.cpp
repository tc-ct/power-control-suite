#include "debug_window.h"

#include <QComboBox>
#include <QEventLoop>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cstring>

#include "device_session_service.h"

namespace {
constexpr int kDebugTimeoutMs = 1000;
}

DebugInterfaceWindow::DebugInterfaceWindow(DeviceSessionService* sessionService, QWidget* parent)
    : QMainWindow(parent), session_service_(sessionService)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle(QStringLiteral("Debug Interface"));
    resize(640, 360);
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

    i2c_bus_combo_ = new QComboBox(i2cGroup);
    i2c_bus_combo_->addItem(QStringLiteral("I2C1"), DEBUG_BUS_I2C1);
    i2c_bus_combo_->addItem(QStringLiteral("I2C2"), DEBUG_BUS_I2C2);

    i2c_slave_id_edit_ = new QLineEdit(i2cGroup);
    i2c_slave_id_edit_->setPlaceholderText(QStringLiteral("0x40"));
    i2c_slave_id_edit_->setText(QStringLiteral("0x40"));

    i2c_reg_addr_edit_ = new QLineEdit(i2cGroup);
    i2c_reg_addr_edit_->setPlaceholderText(QStringLiteral("0x0"));
    i2c_reg_addr_edit_->setText(QStringLiteral("0x0"));

    i2c_addr_len_combo_ = new QComboBox(i2cGroup);
    for (int i = 1; i <= 4; ++i) {
        i2c_addr_len_combo_->addItem(QString::number(i), i);
    }

    i2c_value_len_combo_ = new QComboBox(i2cGroup);
    for (int i = 1; i <= 4; ++i) {
        i2c_value_len_combo_->addItem(QString::number(i), i);
    }
    i2c_value_len_combo_->setCurrentIndex(1);

    i2c_value_edit_ = new QLineEdit(i2cGroup);
    i2c_value_edit_->setPlaceholderText(QStringLiteral("0x00"));

    auto* writeButton = new QPushButton(QStringLiteral("Write"), i2cGroup);
    auto* readButton = new QPushButton(QStringLiteral("Read"), i2cGroup);

    i2c_value_label_ = new QLabel(QStringLiteral("--"), i2cGroup);
    i2c_status_label_ = new QLabel(QStringLiteral("Idle"), i2cGroup);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Status"), i2cGroup), 0, 0);
    i2cGrid->addWidget(i2c_status_label_, 0, 1, 1, 3);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Bus"), i2cGroup), 1, 0);
    i2cGrid->addWidget(i2c_bus_combo_, 1, 1, 1, 3);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Slave Addr(7bit)"), i2cGroup), 2, 0);
    i2cGrid->addWidget(i2c_slave_id_edit_, 2, 1, 1, 3);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Addr Length"), i2cGroup), 3, 0);
    i2cGrid->addWidget(i2c_addr_len_combo_, 3, 1, 1, 3);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Reg Addr"), i2cGroup), 4, 0);
    i2cGrid->addWidget(i2c_reg_addr_edit_, 4, 1, 1, 3);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Value Length"), i2cGroup), 5, 0);
    i2cGrid->addWidget(i2c_value_len_combo_, 5, 1, 1, 3);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Value"), i2cGroup), 6, 0);
    i2cGrid->addWidget(i2c_value_edit_, 6, 1, 1, 3);

    i2cGrid->addWidget(new QLabel(QStringLiteral("Read Value"), i2cGroup), 7, 0);
    i2cGrid->addWidget(i2c_value_label_, 7, 1, 1, 3);

    i2cGrid->addWidget(readButton, 8, 0);
    i2cGrid->addWidget(writeButton, 8, 1);

    auto* listGroup = new QGroupBox(QStringLiteral("Read List"), i2cPage);
    auto* listLayout = new QVBoxLayout(listGroup);
    listLayout->setContentsMargins(6, 6, 6, 6);
    listLayout->setSpacing(4);
    auto* rangeLayout = new QHBoxLayout();
    rangeLayout->setSpacing(4);
    i2c_list_from_edit_ = new QLineEdit(listGroup);
    i2c_list_from_edit_->setPlaceholderText(QStringLiteral("0x00"));
    i2c_list_from_edit_->setText(QStringLiteral("0x00"));
    i2c_list_to_edit_ = new QLineEdit(listGroup);
    i2c_list_to_edit_->setPlaceholderText(QStringLiteral("0x10"));
    i2c_list_to_edit_->setText(QStringLiteral("0x10"));
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
    auto* spiGroup = new QGroupBox(QStringLiteral("SPI Write"), spiPage);
    auto* spiGrid = new QGridLayout(spiGroup);
    spiGrid->setContentsMargins(6, 6, 6, 6);
    spiGrid->setHorizontalSpacing(6);
    spiGrid->setVerticalSpacing(4);

    spi_cs_combo_ = new QComboBox(spiGroup);
    spi_cs_combo_->addItem(QStringLiteral("CS0 (PA4)"), 0);
    spi_cs_combo_->addItem(QStringLiteral("CS1 (PA3)"), 1);
    spi_cs_combo_->addItem(QStringLiteral("CS2 (PA2)"), 2);

    spi_tx_edit_ = new QLineEdit(spiGroup);
    spi_tx_edit_->setPlaceholderText(QStringLiteral("08 00 00 00"));

    auto* spiWriteButton = new QPushButton(QStringLiteral("Send"), spiGroup);
    spi_status_label_ = new QLabel(QStringLiteral("Idle"), spiGroup);

    spiGrid->addWidget(new QLabel(QStringLiteral("Status"), spiGroup), 0, 0);
    spiGrid->addWidget(spi_status_label_, 0, 1, 1, 2);
    spiGrid->addWidget(new QLabel(QStringLiteral("Chip Select"), spiGroup), 1, 0);
    spiGrid->addWidget(spi_cs_combo_, 1, 1, 1, 2);
    spiGrid->addWidget(new QLabel(QStringLiteral("TX Bytes (hex)"), spiGroup), 2, 0);
    spiGrid->addWidget(spi_tx_edit_, 2, 1, 1, 2);
    spiGrid->addWidget(spiWriteButton, 3, 1);

    connect(spiWriteButton, &QPushButton::clicked, this, &DebugInterfaceWindow::onSpiWrite);

    spiLayout->addWidget(spiGroup);
    spiLayout->addStretch(1);

    tabs->addTab(i2cPage, QStringLiteral("i2c debug"));
    tabs->addTab(spiPage, QStringLiteral("spi debug"));
}

bool DebugInterfaceWindow::parseInputU8(QLineEdit* edit, uint8_t& value) const {
    bool ok = false;
    const QString text = edit->text().trimmed();
    const uint32_t parsed = text.toUInt(&ok, 0);
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

bool DebugInterfaceWindow::parseHexBytes(const QString& text, uint8_t* out, uint8_t& outLen) const {
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
        for (const QString& tokenRaw : tokens) {
            QString token = tokenRaw.trimmed();
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

bool DebugInterfaceWindow::sendDebugRequestAndWait(const DebugRequestPacket_t& request,
                                                   DebugResponsePacket_t& response,
                                                   int timeoutMs) {
    if (!session_service_ || !session_service_->isOpen()) {
        return false;
    }
    if (!session_service_->enterDebugSession()) {
        return false;
    }

    QEventLoop loop(this);
    QTimer timer(this);
    timer.setSingleShot(true);

    bool received = false;
    QMetaObject::Connection responseConnection = connect(
        session_service_,
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
    session_service_->exitDebugSession();
    return sent && received;
}

void DebugInterfaceWindow::setI2cStatus(const QString& text) {
    if (i2c_status_label_) {
        i2c_status_label_->setText(text);
    }
}

void DebugInterfaceWindow::setSpiStatus(const QString& text) {
    if (spi_status_label_) {
        spi_status_label_->setText(text);
    }
}

QString DebugInterfaceWindow::formatResponseValue(const DebugResponsePacket_t& response) const {
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

void DebugInterfaceWindow::onI2cWrite() {
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

    if (response.status == DEBUG_STATUS_OK) {
        setI2cStatus(QStringLiteral("I2C write done"));
    } else {
        setI2cStatus(QStringLiteral("I2C write failed: status=%1 err=%2")
                         .arg(static_cast<int>(response.status))
                         .arg(static_cast<int>(response.error_code)));
    }
}

void DebugInterfaceWindow::onI2cRead() {
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
        i2c_value_label_->setText(QStringLiteral("--"));
        setI2cStatus(QStringLiteral("I2C read timeout or send failed"));
        return;
    }

    if (response.status != DEBUG_STATUS_OK) {
        i2c_value_label_->setText(QStringLiteral("--"));
        setI2cStatus(QStringLiteral("I2C read failed: status=%1 err=%2")
                         .arg(static_cast<int>(response.status))
                         .arg(static_cast<int>(response.error_code)));
        return;
    }

    i2c_value_label_->setText(formatResponseValue(response));
    setI2cStatus(QStringLiteral("I2C read done"));
}

void DebugInterfaceWindow::onI2cReadList() {
    if (!session_service_ || !session_service_->isOpen()) {
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
    const uint8_t busId = static_cast<uint8_t>(i2c_bus_combo_->currentData().toInt());
    i2c_list_table_->setRowCount(static_cast<int>(count));

    if (!session_service_->enterDebugSession()) {
        setI2cStatus(QStringLiteral("Debug session failed"));
        return;
    }

    for (uint64_t i = 0; i < count; ++i) {
        const uint32_t addr = static_cast<uint32_t>(fromAddr + i);

        DebugRequestPacket_t request{};
        request.cmd_id = CMD_I2C_READ;
        request.req_id = session_service_->allocateDebugRequestId();
        request.bus_id = busId;
        request.target_id = slaveId;
        request.reg_len = addrLen;
        request.data_len = valueLen;
        request.reg_addr = addr;

        DebugResponsePacket_t response{};
        const bool ok = sendDebugRequestAndWait(request, response, kDebugTimeoutMs);

        auto* addrItem = new QTableWidgetItem(QStringLiteral("0x%1").arg(addr, 8, 16, QLatin1Char('0')).toUpper());
        auto* addrLenItem = new QTableWidgetItem(QString::number(addrLen));
        auto* valueLenItem = new QTableWidgetItem(QString::number(valueLen));
        QTableWidgetItem* valueItem = nullptr;
        if (ok && response.status == DEBUG_STATUS_OK) {
            valueItem = new QTableWidgetItem(formatResponseValue(response));
        } else if (ok) {
            valueItem = new QTableWidgetItem(
                QStringLiteral("ERR(status=%1,err=%2)")
                    .arg(static_cast<int>(response.status))
                    .arg(static_cast<int>(response.error_code)));
        } else {
            valueItem = new QTableWidgetItem(QStringLiteral("--"));
        }

        const int row = static_cast<int>(i);
        i2c_list_table_->setItem(row, 0, addrItem);
        i2c_list_table_->setItem(row, 1, addrLenItem);
        i2c_list_table_->setItem(row, 2, valueLenItem);
        i2c_list_table_->setItem(row, 3, valueItem);
    }

    session_service_->exitDebugSession();
    setI2cStatus(QStringLiteral("I2C read list done"));
}

void DebugInterfaceWindow::onSpiWrite() {
    if (!session_service_ || !session_service_->isOpen()) {
        QMessageBox::warning(this, QStringLiteral("Device not open"), QStringLiteral("Please open device first."));
        setSpiStatus(QStringLiteral("Device not open"));
        return;
    }

    std::array<uint8_t, DEBUG_REQ_MAX_DATA_LEN> txBytes{};
    uint8_t txLen = 0;
    if (!parseHexBytes(spi_tx_edit_->text(), txBytes.data(), txLen)) {
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
