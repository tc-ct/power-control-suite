#include "waveform_widget.h"

#include <QDialog>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QtMath>

#include <algorithm>
#include <array>

WaveformWidget::WaveformWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(180);
    setAutoFillBackground(false);
}

void WaveformWidget::updateFromPacket(const SampleDataPacketTF& packet, const PowersConfig* config)
{
    std::array<bool, SAMPLE_DATA_COUNT> voltageEnabled{};
    std::array<bool, SAMPLE_DATA_COUNT> currentEnabled{};

    if (config == nullptr) {
        return;
    }
    for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
        voltageEnabled[i] = config->sample_cfg[i].volt_en;
        currentEnabled[i] = config->sample_cfg[i].current_en;
    }
    // addVoltageSamples(packet.timestamp, packet.channel_volt_mv, voltageEnabled);
    // addCurrentSamples(packet.timestamp, packet.channel_curr_ma, currentEnabled);

    appendSamples(voltage_series_, voltage_enabled_, packet.timestamp, packet.channel_volt_mv, voltageEnabled);
    appendSamples(current_series_, current_enabled_, packet.timestamp, packet.channel_curr_ma, currentEnabled);
}

// void WaveformWidget::addVoltageSamples(uint32_t timestampMs, const float* values, const std::array<bool, SAMPLE_DATA_COUNT>& enabled)
// {
//     appendSamples(voltage_series_, voltage_enabled_, timestampMs, values, enabled);
// }

// void WaveformWidget::addCurrentSamples(uint32_t timestampMs, const float* values, const std::array<bool, SAMPLE_DATA_COUNT>& enabled)
// {
//     appendSamples(current_series_, current_enabled_, timestampMs, values, enabled);
// }

void WaveformWidget::clearSamples()
{
    for (QVector<QPointF>& points : voltage_series_) {
        points.clear();
    }
    for (QVector<QPointF>& points : current_series_) {
        points.clear();
    }
    voltage_enabled_.fill(false);
    current_enabled_.fill(false);
    syncExpandedView();
    update();
}

void WaveformWidget::appendSamples(
    std::array<QVector<QPointF>, SAMPLE_DATA_COUNT>& series,
    std::array<bool, SAMPLE_DATA_COUNT>& activeFlags,
    uint32_t timestampMs,
    const float* values,
    const std::array<bool, SAMPLE_DATA_COUNT>& enabled)
{
    if (values == nullptr) {
        return;
    }

    const qreal timestampSeconds = static_cast<qreal>(timestampMs) / 1000.0;

    for (int channel = 0; channel < SAMPLE_DATA_COUNT; ++channel) {
        activeFlags[channel] = enabled[channel];
        QVector<QPointF>& points = series[channel];
        if (!enabled[channel]) {
            points.clear();
            continue;
        }

        points.push_back(QPointF(timestampSeconds, static_cast<qreal>(values[channel])));
        if (points.size() > max_points_) {
            points.remove(0, points.size() - max_points_);
        }
    }

    syncExpandedView();
    update();
}

void WaveformWidget::syncExpandedView()
{
    if (!expanded_widget_) {
        return;
    }

    expanded_widget_->voltage_series_ = voltage_series_;
    expanded_widget_->current_series_ = current_series_;
    expanded_widget_->voltage_enabled_ = voltage_enabled_;
    expanded_widget_->current_enabled_ = current_enabled_;
    expanded_widget_->max_points_ = max_points_;
    expanded_widget_->update();
}

void WaveformWidget::openExpandedView()
{
    if (is_expanded_view_) {
        return;
    }

    if (!expanded_dialog_) {
        expanded_dialog_ = new QDialog(this, Qt::Window);
        expanded_dialog_->setAttribute(Qt::WA_DeleteOnClose, false);
        expanded_dialog_->setWindowTitle(QStringLiteral("放大波形图"));
        expanded_dialog_->resize(1280, 720);

        auto* layout = new QVBoxLayout(expanded_dialog_);
        layout->setContentsMargins(8, 8, 8, 8);

        connect(expanded_dialog_, &QDialog::finished, this, [this]() {
            update();
        });

        connect(expanded_dialog_, &QObject::destroyed, this, [this]() {
            expanded_dialog_ = nullptr;
            expanded_widget_ = nullptr;
            update();
        });
    }

    if (!expanded_widget_) {
        auto* layout = qobject_cast<QVBoxLayout*>(expanded_dialog_->layout());
        if (!layout) {
            layout = new QVBoxLayout(expanded_dialog_);
            layout->setContentsMargins(8, 8, 8, 8);
        }

        expanded_widget_ = new WaveformWidget(expanded_dialog_);
        expanded_widget_->is_expanded_view_ = true;
        expanded_widget_->setMinimumSize(1100, 620);
        layout->addWidget(expanded_widget_);
    }

    syncExpandedView();
    expanded_dialog_->show();
    expanded_dialog_->raise();
    expanded_dialog_->activateWindow();
    update();
}

void WaveformWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF fullRect = rect();
    painter.fillRect(fullRect, QColor(247, 250, 253));

    const qreal leftPadding = 64.0;
    const qreal rightPadding = 18.0;
    const qreal topPadding = 16.0;
    const qreal bottomPadding = 34.0;
    const qreal panelGap = 18.0;
    const QRectF contentRect(
        leftPadding,
        topPadding,
        qMax(10.0, fullRect.width() - leftPadding - rightPadding),
        qMax(10.0, fullRect.height() - topPadding - bottomPadding));

    const qreal panelHeight = qMax(40.0, (contentRect.height() - panelGap) / 2.0);
    const QRectF voltageRect(contentRect.left(), contentRect.top(), contentRect.width(), panelHeight);
    const QRectF currentRect(contentRect.left(), voltageRect.bottom() + panelGap, contentRect.width(), panelHeight);

    painter.setPen(QPen(QColor(214, 223, 232), 1));
    painter.drawRect(voltageRect);
    painter.drawRect(currentRect);

    painter.setPen(QColor(84, 99, 115));
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
//    painter.drawText(QPointF(contentRect.left(), contentRect.top() - 4), QStringLiteral("Waveform"));
    painter.drawText(QPointF(contentRect.left(), fullRect.bottom() - 8), QStringLiteral("Time (s)"));

    const bool hideOriginalWaveform = !is_expanded_view_ && expanded_dialog_ && expanded_dialog_->isVisible();
    if (hideOriginalWaveform) {
        painter.setPen(QColor(120, 131, 143));
        painter.drawText(contentRect, Qt::AlignCenter, QStringLiteral("波形已在放大窗口中显示"));
        return;
    }

    auto hasActiveSeries = [](const std::array<QVector<QPointF>, SAMPLE_DATA_COUNT>& series,
                              const std::array<bool, SAMPLE_DATA_COUNT>& enabled) {
        for (int channel = 0; channel < SAMPLE_DATA_COUNT; ++channel) {
            if (enabled[channel] && !series[channel].isEmpty()) {
                return true;
            }
        }
        return false;
    };

    const bool hasVoltageData = hasActiveSeries(voltage_series_, voltage_enabled_);
    const bool hasCurrentData = hasActiveSeries(current_series_, current_enabled_);
    if (!hasVoltageData && !hasCurrentData) {
        painter.setPen(QColor(120, 131, 143));
        painter.drawText(contentRect, Qt::AlignCenter, QStringLiteral("等待采样数据..."));
        return;
    }

    auto colorForChannel = [](int channel) {
        return QColor::fromHsv((channel * 37) % 360, 190, 210);
    };

    auto computeValueRange = [](const std::array<QVector<QPointF>, SAMPLE_DATA_COUNT>& series,
                           const std::array<bool, SAMPLE_DATA_COUNT>& enabled) {
        bool initialized = false;
        float minV = 0.0f;
        float maxV = 0.0f;
        for (int channel = 0; channel < SAMPLE_DATA_COUNT; ++channel) {
            if (!enabled[channel]) {
                continue;
            }
            for (const QPointF& point : series[channel]) {
                const float value = static_cast<float>(point.y());
                if (!initialized) {
                    minV = value;
                    maxV = value;
                    initialized = true;
                } else {
                    minV = qMin(minV, value);
                    maxV = qMax(maxV, value);
                }
            }
        }
        if (!initialized) {
            return qMakePair(0.0f, 1.0f);
        }
        const float span = qMax(1.0f, maxV - minV);
        const float margin = span * 0.1f;
        return qMakePair(minV - margin, maxV + margin);
    };

    auto computeTimeRange = [](const std::array<QVector<QPointF>, SAMPLE_DATA_COUNT>& series,
                               const std::array<bool, SAMPLE_DATA_COUNT>& enabled) {
        bool initialized = false;
        qreal minTime = 0.0;
        qreal maxTime = 0.0;
        for (int channel = 0; channel < SAMPLE_DATA_COUNT; ++channel) {
            if (!enabled[channel]) {
                continue;
            }
            for (const QPointF& point : series[channel]) {
                if (!initialized) {
                    minTime = point.x();
                    maxTime = point.x();
                    initialized = true;
                } else {
                    minTime = qMin(minTime, point.x());
                    maxTime = qMax(maxTime, point.x());
                }
            }
        }
        return qMakePair(minTime, maxTime);
    };

    auto voltageTimeRange = computeTimeRange(voltage_series_, voltage_enabled_);
    auto currentTimeRange = computeTimeRange(current_series_, current_enabled_);
    qreal minTime = 0.0;
    qreal maxTime = 1.0;
    bool hasTimeRange = false;
    if (hasVoltageData) {
        minTime = voltageTimeRange.first;
        maxTime = voltageTimeRange.second;
        hasTimeRange = true;
    }
    if (hasCurrentData) {
        if (!hasTimeRange) {
            minTime = currentTimeRange.first;
            maxTime = currentTimeRange.second;
            hasTimeRange = true;
        } else {
            minTime = qMin(minTime, currentTimeRange.first);
            maxTime = qMax(maxTime, currentTimeRange.second);
        }
    }
    if (!hasTimeRange || qFuzzyCompare(minTime, maxTime)) {
        maxTime = minTime + 1.0;
    }

    auto buildPath = [minTime, maxTime](const QVector<QPointF>& points, const QRectF& plotRect, float minValue, float maxValue) {
        QPainterPath path;
        if (points.size() < 2) {
            return path;
        }
        const qreal timeSpan = qMax(1e-6, maxTime - minTime);
        const qreal denom = qMax(1e-6, static_cast<double>(maxValue - minValue));
        for (int i = 0; i < points.size(); ++i) {
            const qreal x = plotRect.left() + ((points[i].x() - minTime) / timeSpan) * plotRect.width();
            const qreal ratio = qBound(0.0, static_cast<double>((points[i].y() - minValue) / denom), 1.0);
            const qreal y = plotRect.bottom() - ratio * plotRect.height();
            if (i == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }
        }
        return path;
    };

    auto drawPanel = [&](const QRectF& plotRect,
                         const QString& title,
                         const QString& unit,
                         const std::array<QVector<QPointF>, SAMPLE_DATA_COUNT>& series,
                         const std::array<bool, SAMPLE_DATA_COUNT>& enabled) {
        painter.setPen(QPen(QColor(228, 235, 242), 1, Qt::DashLine));
        for (int i = 1; i < 4; ++i) {
            const qreal y = plotRect.top() + plotRect.height() * (static_cast<qreal>(i) / 4.0);
            painter.drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));
        }

        const auto range = computeValueRange(series, enabled);
        const int yTicks = 4;
        painter.setPen(QColor(100, 116, 139));
        for (int i = 0; i <= yTicks; ++i) {
            const qreal t = static_cast<qreal>(i) / yTicks;
            const qreal y = plotRect.bottom() - t * plotRect.height();
            const float value = range.first + static_cast<float>(t) * (range.second - range.first);
            painter.drawText(
                QRectF(2, y - 8, leftPadding - 8, 16),
                Qt::AlignRight | Qt::AlignVCenter,
                QStringLiteral("%1%2").arg(QString::number(value, 'f', 0), unit));
        }

        painter.setPen(QColor(84, 99, 115));
        painter.drawText(QRectF(plotRect.left(), plotRect.top() - 14, 140, 14), Qt::AlignLeft | Qt::AlignVCenter, title);

        for (int channel = 0; channel < SAMPLE_DATA_COUNT; ++channel) {
            if (!enabled[channel] || series[channel].size() < 2) {
                continue;
            }

            QPen linePen(colorForChannel(channel), 1.6);
            painter.setPen(linePen);
            painter.drawPath(buildPath(series[channel], plotRect, range.first, range.second));
        }
    };

    if (hasVoltageData) {
        drawPanel(voltageRect, QStringLiteral("Voltage (mV)"), QStringLiteral("mV"), voltage_series_, voltage_enabled_);
    } else {
        painter.setPen(QColor(148, 163, 184));
        painter.drawText(voltageRect, Qt::AlignCenter, QStringLiteral("无已启用电压波形"));
    }

    if (hasCurrentData) {
        drawPanel(currentRect, QStringLiteral("Current (mA)"), QStringLiteral("mA"), current_series_, current_enabled_);
    } else {
        painter.setPen(QColor(148, 163, 184));
        painter.drawText(currentRect, Qt::AlignCenter, QStringLiteral("无已启用电流波形"));
    }

    const int xTicks = 5;
    painter.setPen(QColor(84, 99, 115));
    for (int i = 0; i <= xTicks; ++i) {
        const qreal t = static_cast<qreal>(i) / xTicks;
        const qreal x = contentRect.left() + t * contentRect.width();
        const qreal seconds = minTime + (maxTime - minTime) * t;
        painter.drawLine(QPointF(x, currentRect.bottom()), QPointF(x, currentRect.bottom() + 4));
        painter.drawText(
            QRectF(x - 24, currentRect.bottom() + 6, 48, 16),
            Qt::AlignHCenter | Qt::AlignTop,
            QString::number(seconds, 'f', 3));
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(22, 163, 74));
    painter.drawEllipse(QRectF(contentRect.right() - 120, contentRect.top() + 4, 7, 7));
    painter.setPen(QColor(56, 68, 82));
    painter.drawText(QRectF(contentRect.right() - 108, contentRect.top() - 3, 110, 16), QStringLiteral("按通道分别绘制"));
}

void WaveformWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (!is_expanded_view_ && event && event->button() == Qt::LeftButton) {
        openExpandedView();
        event->accept();
        return;
    }

    QWidget::mouseDoubleClickEvent(event);
}
