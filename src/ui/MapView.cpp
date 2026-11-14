#include "ui/MapView.h"

#include "data/DatasetRegistry.h"
#include "data/RasterLayer.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPointF>
#include <QShowEvent>
#include <QWheelEvent>

#include <cmath>

namespace {
// M1b zoom clamp in metres per pixel: sub-metre detail (HiRISE-scale) at the
// fine end, regional context at the coarse end. Never zoom past these.
constexpr double kMinMpp = 0.25;
constexpr double kMaxMpp = 200.0;

// Fit margin on each side (5%): the extent occupies 90% of the widget.
constexpr double kFitMargin = 0.05;

// High-contrast overlay colours on the dark regolith backdrop.
constexpr QColor kLightText(245, 240, 232, 255);
constexpr QColor kDarkBar(8, 6, 5, 170);

double clampMpp(double mpp)
{
    return qBound(kMinMpp, mpp, kMaxMpp);
}

// Round `raw` metres to a 1/2/2.5/5/10 x 10^k value for the scale-bar label
// (e.g. 120 m -> 100 m, 240 m -> 250 m).
double niceMeters(double raw)
{
    if (!(raw > 0.0))
        return 1.0;
    const double base = std::pow(10.0, std::floor(std::log10(raw)));
    const double candidates[] = {1.0, 2.0, 2.5, 5.0, 10.0};
    double best = candidates[0] * base;
    double bestDist = std::abs(raw - best);
    for (double c : candidates) {
        const double v = c * base;
        const double d = std::abs(raw - v);
        if (d < bestDist) {
            best = v;
            bestDist = d;
        }
    }
    return best;
}

// North arrow at the top-right: shaft + head + "N".
void drawNorthArrow(QPainter &painter, const QRect &widgetRect)
{
    const int x = widgetRect.right() - 30;
    const int top = widgetRect.top() + 12;
    painter.fillRect(QRect(x - 18, top - 4, 36, 70), kDarkBar);
    painter.setPen(kLightText);
    painter.drawText(QRect(x - 18, top, 36, 16), Qt::AlignCenter,
                     QStringLiteral("N"));
    const int yBase = top + 58;
    const int yTip = top + 26;
    painter.setPen(QPen(kLightText, 2));
    painter.drawLine(x, yBase, x, yTip);
    painter.setPen(Qt::NoPen);
    painter.setBrush(kLightText);
    QPolygonF head;
    head << QPointF(x, yTip - 8) << QPointF(x - 6, yTip + 6)
         << QPointF(x + 6, yTip + 6);
    painter.drawPolygon(head);
}

// Scale bar at the bottom-left over a dark translucent backing.
void drawScaleBar(QPainter &painter, const QRect &widgetRect, double mpp)
{
    // Label is a round number of metres derived from mpp * 120 (spec).
    const double niceM = niceMeters(mpp * 120.0);
    const int barW = qMax(8, int(std::lround(niceM / mpp)));
    const int x0 = 16;
    const int boxTop = widgetRect.bottom() - 62;
    const int yBar = boxTop + 36;
    painter.fillRect(QRect(8, boxTop, barW + 44,
                           (widgetRect.bottom() - 21) - boxTop),
                     kDarkBar);
    painter.setPen(kLightText);
    painter.drawText(QRect(x0, boxTop + 2, barW + 28, 18),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("%1 m")
                         .arg(QString::number(niceM, 'g', 3)));
    painter.setPen(QPen(kLightText, 2));
    painter.drawLine(x0, yBar, x0 + barW, yBar);
    painter.drawLine(x0, yBar - 5, x0, yBar + 5);
    painter.drawLine(x0 + barW, yBar - 5, x0 + barW, yBar + 5);
}

// One attribution line at the bottom: "<source> · <resolution> · <units>".
void drawAttribution(QPainter &painter, const QRect &widgetRect,
                     const RasterLayer::Metadata &md)
{
    const QRect bar(widgetRect.left(), widgetRect.bottom() - 20,
                    widgetRect.width(), 21);
    painter.fillRect(bar, kDarkBar);
    painter.setPen(kLightText);
    painter.drawText(bar.adjusted(10, 0, -10, 0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("%1 · %2 · %3")
                         .arg(md.source, md.resolutionText, md.units));
}
}  // namespace

MapView::MapView(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(24, 18, 15)); // dark regolith backdrop
    setPalette(pal);
}

QSize MapView::sizeHint() const
{
    return QSize(900, 640);
}

void MapView::setRegistry(const DatasetRegistry *registry)
{
    m_registry = registry;
    if (m_registry && m_registry->defaultLayer()) {
        m_activeLayerId = m_registry->defaultLayer()->metadata().id;
        fitToScene();
    } else {
        m_activeLayerId.clear();
        update();
        emit viewChanged();
    }
}

void MapView::setActiveLayer(const QString &layerId)
{
    if (!m_registry || !m_registry->layer(layerId))
        return;  // unknown ids are ignored
    m_activeLayerId = layerId;
    fitToScene();
}

QString MapView::activeLayerId() const
{
    return m_activeLayerId;
}

void MapView::fitToScene()
{
    QRectF extent;
    if (m_registry) {
        const RasterLayer *active = activeLayer();
        if (active) {
            extent = active->info().extent;
        } else {
            // No active layer: union of every registry layer extent.
            for (const auto &layer : m_registry->layers()) {
                if (layer && layer->isValid())
                    extent = extent.united(layer->info().extent);
            }
        }
    }
    if (extent.width() > 0.0 && extent.height() > 0.0) {
        m_center = extent.center();
        if (width() > 0 && height() > 0) {
            const double usableW = width() * (1.0 - 2.0 * kFitMargin);
            const double usableH = height() * (1.0 - 2.0 * kFitMargin);
            // Per-axis fit in pixels/metre; qMin of the x/y fit is the
            // tighter fit and, inverted, guarantees the extent fits on
            // both axes with the 5% margin.
            const double fitX = usableW / extent.width();
            const double fitY = usableH / extent.height();
            m_mpp = clampMpp(1.0 / qMin(fitX, fitY));
        }
    }
    update();
    emit viewChanged();
}

void MapView::setMetersPerPixel(double metersPerPixel)
{
    m_mpp = clampMpp(metersPerPixel);
    update();
    emit viewChanged();
}

double MapView::metersPerPixel() const
{
    return m_mpp;
}

QRectF MapView::visibleExtentMeters() const
{
    const double w = width() * m_mpp;
    const double h = height() * m_mpp;
    return QRectF(m_center.x() - w / 2.0, m_center.y() - h / 2.0, w, h);
}

void MapView::zoomIn()
{
    m_mpp = clampMpp(m_mpp / 1.25);
    update();
    emit viewChanged();
}

void MapView::zoomOut()
{
    m_mpp = clampMpp(m_mpp * 1.25);
    update();
    emit viewChanged();
}

const RasterLayer *MapView::activeLayer() const
{
    if (!m_registry || m_activeLayerId.isEmpty())
        return nullptr;
    return m_registry->layer(m_activeLayerId);
}

QPointF MapView::pixelToMeters(const QPointF &pixel) const
{
    const QRectF extent = visibleExtentMeters();
    // Spec: east = left + x*mpp, north = bottom - y*mpp.
    return QPointF(extent.left() + pixel.x() * m_mpp,
                   extent.bottom() - pixel.y() * m_mpp);
}

void MapView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.fillRect(rect(), QColor(24, 18, 15)); // regolith backdrop

    const RasterLayer *layer = activeLayer();
    if (!m_registry || !layer) {
        painter.setPen(QColor(214, 205, 195));
        painter.drawText(rect(), Qt::AlignCenter,
                         tr("MAP VIEW — no layers loaded"));
        return;
    }

    // readImage is north-up (row 0 = north edge): draw as-is, never flip.
    const QImage img = layer->readImage(visibleExtentMeters(), size());
    if (!img.isNull())
        painter.drawImage(QPoint(0, 0), img);

    drawNorthArrow(painter, rect());
    drawScaleBar(painter, rect(), m_mpp);
    drawAttribution(painter, rect(), layer->metadata());
}

void MapView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_didInitialFit) {
        m_didInitialFit = true;
        if (m_registry && !m_activeLayerId.isEmpty())
            fitToScene();
    }
}

void MapView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_panning = true;
        m_lastMousePos = event->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void MapView::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    if (m_panning) {
        const QPoint delta = pos - m_lastMousePos;
        m_lastMousePos = pos;
        // Drag right moves the map east -> centre moves west; drag down
        // moves the centre north (y grows north).
        m_center -= QPointF(delta.x() * m_mpp, -delta.y() * m_mpp);
        update();
        emit viewChanged();
    }
    const QPointF m = pixelToMeters(pos);
    emit cursorMoved(m.x(), m.y());
    event->accept();
}

void MapView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_panning) {
        m_panning = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void MapView::wheelEvent(QWheelEvent *event)
{
    // Zoom about the cursor: keep the projected point under the cursor
    // fixed while mpp changes.
    const QPointF cursorPixel = event->position();
    const QPointF before = pixelToMeters(cursorPixel);
    m_mpp = clampMpp(m_mpp * std::pow(1.0015, -event->angleDelta().y()));
    const QPointF after = pixelToMeters(cursorPixel);  // same centre, new mpp
    m_center += before - after;
    update();
    emit viewChanged();
    event->accept();
}
