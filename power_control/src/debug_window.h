#ifndef DEBUG_WINDOW_H
#define DEBUG_WINDOW_H

#include <QMainWindow>

#include <cstdint>

#include "proto_pkg.h"

class QLabel;
class QComboBox;
class QLineEdit;
class QTableWidget;
class DeviceSessionService;

class DebugInterfaceWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit DebugInterfaceWindow(DeviceSessionService* sessionService, QWidget* parent = nullptr);

private slots:
	void onI2cWrite();
	void onI2cRead();
	void onI2cReadList();
	void onSpiWrite();

private:
	void buildUi();
	bool parseInputU8(QLineEdit* edit, uint8_t &value) const;
	bool parseInputU32(QLineEdit* edit, uint32_t &value) const;
	bool parseHexBytes(const QString& text, uint8_t* out, uint8_t &outLen) const;
	bool sendDebugRequestAndWait(const DebugRequestPacket_t& request, DebugResponsePacket_t& response, int timeoutMs = 1000);
	void setI2cStatus(const QString& text);
	void setSpiStatus(const QString& text);
	QString formatResponseValue(const DebugResponsePacket_t& response) const;

	DeviceSessionService* session_service_ = nullptr;

	QComboBox *i2c_bus_combo_ = nullptr;
	QLineEdit *i2c_slave_id_edit_ = nullptr;
	QLineEdit *i2c_reg_addr_edit_ = nullptr;
	QComboBox *i2c_addr_len_combo_ = nullptr;
	QComboBox *i2c_value_len_combo_ = nullptr;
	QLineEdit *i2c_value_edit_ = nullptr;
	QLineEdit *i2c_list_from_edit_ = nullptr;
	QLineEdit *i2c_list_to_edit_ = nullptr;
	QTableWidget *i2c_list_table_ = nullptr;
	QLabel *i2c_value_label_ = nullptr;
	QLabel *i2c_status_label_ = nullptr;

	QComboBox *spi_cs_combo_ = nullptr;
	QLineEdit *spi_tx_edit_ = nullptr;
	QLabel *spi_status_label_ = nullptr;
};

#endif // DEBUG_WINDOW_H
