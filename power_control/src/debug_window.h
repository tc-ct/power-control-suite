#ifndef debug_window_H
#define debug_window_H

#include <QMainWindow>
#include <functional>

class QLabel;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
class USBDriver;

class DebugInterfaceWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit DebugInterfaceWindow(std::function<USBDriver*()> usbProvider, QWidget* parent = nullptr);

private slots:
	void onI2cWrite();
	void onI2cRead();
	void onI2cReadList();

private:
	void buildUi();
	bool parseInputU8(QLineEdit* edit, uint8_t &value) const;
	bool parseInputU32(QLineEdit* edit, uint32_t &value) const;
	bool sendI2cReadRequest(uint8_t slaveId, uint32_t addr, uint8_t addrLen, uint32_t &outValue);
	void setI2cStatus(const QString& text);

	std::function<USBDriver*()> usb_provider_;

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
};

#endif // debug_window_H
