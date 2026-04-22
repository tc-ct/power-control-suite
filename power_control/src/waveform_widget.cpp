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
    time_origin_initialized_ = false;
    time_origin_seconds_ = 0.0;
    latest_time_seconds_ = 0.0;
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
    if (!time_origin_initialized_ || timestampSeconds < time_origin_seconds_) {
        time_origin_seconds_ = timestampSeconds;
        time_origin_initialized_ = true;
    }
    const qreal relativeSeconds = qMax(0.0, timestampSeconds - time_origin_seconds_);
    latest_time_seconds_ = qMax(latest_time_seconds_, relativeSeconds);

    for (int channel = 0; channel < SAMPLE_DATA_COUNT; ++channel) {
        activeFlags[channel] = enabled[channel];
        QVector<QPointF>& points = series[channel];
        if (!enabled[channel]) {
            points.clear();
            continue;
        }

        points.push_back(QPointF(relativeSeconds, static_cast<qreal>(values[channel])));
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
    expanded_widget_->time_origin_initialized_ = time_origin_initialized_;
    expanded_widget_->time_origin_seconds_ = time_origin_seconds_;
    expanded_widget_->latest_time_seconds_ = latest_time_seconds_;
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

    const qreal latestTime = qMax(0.0, latest_time_seconds_);
    qreal minTime = 0.0;
    qreal maxTime = kTimeWindowSeconds;
    if (latestTime > kTimeWindowSeconds) {
        minTime = latestTime - kTimeWindowSeconds;
        maxTime = latestTime;
    }

    auto buildPath = [minTime, maxTime](const QVector<QPointF>& points, const QRectF& plotRect, float minValue, float maxValue) {
        QPainterPath path;
        if (points.size() < 2) {
            return path;
        }
        const qreal timeSpan = qMax(1e-6, maxTime - minTime);
        const qreal denom = qMax(1e-6, static_cast<double>(maxValue - minValue));

        auto mapToPlot = [&](const QPointF& p) {
            const qreal xRatio = qBound(0.0, static_cast<double>((p.x() - minTime) / timeSpan), 1.0);
            const qreal x = plotRect.left() + xRatio * plotRect.width();
            const qreal yRatio = qBound(0.0, static_cast<double>((p.y() - minValue) / denom), 1.0);
            const qreal y = plotRect.bottom() - yRatio * plotRect.height();
            return QPointF(x, y);
        };

        auto clipSegmentByTimeWindow = [minTime, maxTime](const QPointF& in0, const QPointF& in1, QPointF* out0, QPointF* out1) {
            const qreal x0 = in0.x();
            const qreal x1 = in1.x();
            if ((x0 < minTime && x1 < minTime) || (x0 > maxTime && x1 > maxTime)) {
                return false;
            }

            QPointF p0 = in0;
            QPointF p1 = in1;
            const qreal dx = x1 - x0;
            if (!qFuzzyIsNull(dx)) {
                if (p0.x() < minTime) {
                    const qreal t = (minTime - x0) / dx;
                    p0.setX(minTime);
                    p0.setY(in0.y() + (in1.y() - in0.y()) * t);
                } else if (p0.x() > maxTime) {
                    const qreal t = (maxTime - x0) / dx;
                    p0.setX(maxTime);
                    p0.setY(in0.y() + (in1.y() - in0.y()) * t);
                }

                if (p1.x() < minTime) {
                    const qreal t = (minTime - x0) / dx;
                    p1.setX(minTime);
                    p1.setY(in0.y() + (in1.y() - in0.y()) * t);
                } else if (p1.x() > maxTime) {
                    const qreal t = (maxTime - x0) / dx;
                    p1.setX(maxTime);
                    p1.setY(in0.y() + (in1.y() - in0.y()) * t);
                }
            } else if (x0 < minTime || x0 > maxTime) {
                return false;
            }

            *out0 = p0;
            *out1 = p1;
            return true;
        };

        bool hasSubPath = false;
        QPointF lastEnd;
        for (int i = 1; i < points.size(); ++i) {
            QPointF clipped0;
            QPointF clipped1;
            if (!clipSegmentByTimeWindow(points[i - 1], points[i], &clipped0, &clipped1)) {
                hasSubPath = false;
                continue;
            }

            const QPointF start = mapToPlot(clipped0);
            const QPointF end = mapToPlot(clipped1);
            const bool continuous = hasSubPath
                && qFuzzyCompare(start.x() + 1.0, lastEnd.x() + 1.0)
                && qFuzzyCompare(start.y() + 1.0, lastEnd.y() + 1.0);

            if (!continuous) {
                path.moveTo(start);
            }
            path.lineTo(end);
            lastEnd = end;
            hasSubPath = true;
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

        painter.save();
        // Keep waveform rendering strictly inside the panel frame.
        painter.setClipRect(plotRect.adjusted(0.5, 0.5, -0.5, -0.5));
        for (int channel = 0; channel < SAMPLE_DATA_COUNT; ++channel) {
            if (!enabled[channel] || series[channel].size() < 2) {
                continue;
            }

            QPen linePen(colorForChannel(channel), 1.6);
            painter.setPen(linePen);
            painter.drawPath(buildPath(series[channel], plotRect, range.first, range.second));
        }
        painter.restore();
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
