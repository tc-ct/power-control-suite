#ifndef WAVEFORM_WIDGET_H
#define WAVEFORM_WIDGET_H

#include <QWidget>
#include <QPointF>
#include <QVector>
#include <QPointer>

#include <array>

#include "power_config.h"

class QDialog;

class WaveformWidget : public QWidget
{
public:
	explicit WaveformWidget(QWidget* parent = nullptr);

	void updateFromPacket(const SampleDataPacketTF& packet, const PowersConfig* config);
	void clearSamples();

protected:
	void paintEvent(QPaintEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
	void appendSamples(
		std::array<QVector<QPointF>, SAMPLE_DATA_COUNT> &series,
		std::array<bool, SAMPLE_DATA_COUNT> &activeFlags,
		uint32_t timestampMs,
		const float *values,
		const std::array<bool, SAMPLE_DATA_COUNT> &enabled);
	void syncExpandedView();
	void openExpandedView();

    std::array<QVector<QPointF>, SAMPLE_DATA_COUNT> voltage_series_;
    std::array<QVector<QPointF>, SAMPLE_DATA_COUNT> current_series_;
    std::array<bool, SAMPLE_DATA_COUNT> voltage_enabled_{};
    std::array<bool, SAMPLE_DATA_COUNT> current_enabled_{};
    bool time_origin_initialized_ = false;
    qreal time_origin_seconds_ = 0.0;
    qreal latest_time_seconds_ = 0.0;
    static constexpr qreal kTimeWindowSeconds = 10.0;
    int max_points_ = 240;
    bool is_expanded_view_ = false;
    QPointer<QDialog> expanded_dialog_;
    WaveformWidget* expanded_widget_ = nullptr;
};

#endif // WAVEFORM_WIDGET_H
