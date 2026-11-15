#include "ui/MapView.h"

#include "data/DatasetRegistry.h"
#include "data/RasterLayer.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPointF>
#include <QPolygonF>
#include <QShowEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <vector>

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

// M3 overlay colours. Hazard grid is drawn at low opacity so the imagery
// still reads underneath; route gets a dark casing plus a cyan core.
constexpr QColor kHazardTint(220, 40, 30, 255);
constexpr QColor kRouteCasing(4, 10, 14, 210);
constexpr QColor kRouteCore(0, 220, 255);
constexpr QColor kPlanRouteCore(150, 235, 255);
constexpr QColor kCrewGreen(80, 210, 90);
constexpr QColor kTargetAmber(255, 190, 60);
constexpr QColor kTargetRed(220, 60, 50);
constexpr QColor kPrimaryOrange(255, 140, 0);

// M3 fit-to-route context floor: even a short traverse keeps at least this
// much visible span per axis so the crew sees the surrounding terrain.
constexpr double kFitRouteMinSpanM = 500.0;
constexpr double kFitRouteMargin = 0.15;
// Labels sit slightly off the marker over a translucent dark backing.
constexpr int kLabelOffsetX = 12;
constexpr int kLabelOffsetY = 8;

double clampMpp(double mpp)
{
    return qBound(kMinMpp, mpp, kMaxMpp);
}

// M2 bottom-to-top draw order: Optical -> Hillshade -> Elevation -> Slope.
// This is independent of RasterLayer::Kind's declaration order.
int kindDrawRank(RasterLayer::Kind kind)
{
    switch (kind) {
    case RasterLayer::Kind::Optical:
        return 0;
    case RasterLayer::Kind::Hillshade:
        return 1;
    case RasterLayer::Kind::Elevation:
        return 2;
    case RasterLayer::Kind::Slope:
        return 3;
    }
    return 4;
}

// Resolve visible ids against the registry and sort them bottom-to-top.
std::vector<const RasterLayer *> orderedVisibleLayers(
    const DatasetRegistry *registry, const QStringList &visibleIds)
{
    std::vector<const RasterLayer *> layers;
    if (!registry)
        return layers;
    layers.reserve(size_t(visibleIds.size()));
    for (const QString &id : visibleIds) {
        if (const RasterLayer *layer = registry->layer(id))
            layers.push_back(layer);
    }
    std::stable_sort(layers.begin(), layers.end(),
                     [](const RasterLayer *a, const RasterLayer *b) {
                         return kindDrawRank(a->metadata().kind)
                                < kindDrawRank(b->metadata().kind);
                     });
    return layers;
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
    const QStringList oldVisible = m_visibleLayerIds;
    m_visibleLayerIds.clear();
    if (m_registry && m_registry->defaultLayer()) {
        m_activeLayerId = m_registry->defaultLayer()->metadata().id;
        m_visibleLayerIds = QStringList{m_activeLayerId};
        fitToScene();
    } else {
        m_activeLayerId.clear();
        update();
        emit viewChanged();
    }
    if (m_visibleLayerIds != oldVisible)
        emit visibleLayersChanged();
}

void MapView::setActiveLayer(const QString &layerId)
{
    if (!m_registry || !m_registry->layer(layerId))
        return;  // unknown ids are ignored
    const QStringList oldVisible = m_visibleLayerIds;
    m_activeLayerId = layerId;
    // M1 solo semantics: selecting a layer makes it the only visible one.
    m_visibleLayerIds = QStringList{layerId};
    fitToScene();
    if (m_visibleLayerIds != oldVisible)
        emit visibleLayersChanged();
}

void MapView::setLayerVisible(const QString &layerId, bool visible)
{
    if (!m_registry || !m_registry->layer(layerId)) {
        // Unknown ids / no registry are ignored, but the spec still wants a
        // repaint request.
        update();
        return;
    }
    const bool wasVisible = m_visibleLayerIds.contains(layerId);
    bool changed = false;
    if (visible && !wasVisible) {
        m_visibleLayerIds.append(layerId);
        changed = true;
    } else if (!visible && wasVisible) {
        m_visibleLayerIds.removeAll(layerId);
        changed = true;
    }
    if (changed)
        emit visibleLayersChanged();
    update();
}

bool MapView::isLayerVisible(const QString &layerId) const
{
    return m_visibleLayerIds.contains(layerId);
}

QStringList MapView::visibleLayerIds() const
{
    QStringList ids;
    for (const RasterLayer *layer : orderedVisibleLayers(m_registry, m_visibleLayerIds))
        ids.append(layer->metadata().id);
    return ids;
}

void MapView::showAllLayers()
{
    if (!m_registry)
        return;
    const QStringList ids = m_registry->ids();
    const bool changed = (ids != m_visibleLayerIds);
    m_visibleLayerIds = ids;
    update();
    if (changed)
        emit visibleLayersChanged();
}

void MapView::hideAllLayers()
{
    const bool changed = !m_visibleLayerIds.isEmpty();
    m_visibleLayerIds.clear();
    update();
    if (changed)
        emit visibleLayersChanged();
}

QString MapView::activeLayerId() const
{
    return m_activeLayerId;
}

void MapView::fitToScene()
{
    QRectF extent;
    if (m_registry) {
        // M2: fit the union of visible layer extents. With nothing visible,
        // fall back to the active layer, then to every registry extent.
        const std::vector<const RasterLayer *> visible =
            orderedVisibleLayers(m_registry, m_visibleLayerIds);
        for (const RasterLayer *layer : visible) {
            if (layer->isValid())
                extent = extent.united(layer->info().extent);
        }
        if (extent.width() <= 0.0 || extent.height() <= 0.0) {
            const RasterLayer *active = activeLayer();
            if (active && active->isValid()) {
                extent = active->info().extent;
            } else {
                for (const auto &layer : m_registry->layers()) {
                    if (layer && layer->isValid())
                        extent = extent.united(layer->info().extent);
                }
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

void MapView::setRoute(const Route &route)
{
    m_route = route;
    update();
}

void MapView::clearRoute()
{
    m_route = Route();
    update();
}

void MapView::setPlanRoutes(const QVector<Route> &routes)
{
    m_planRoutes = routes;
    update();
}

void MapView::clearPlanRoutes()
{
    m_planRoutes.clear();
    update();
}

void MapView::setTargets(const QVector<RankedTarget> &targets,
                         const QString &primaryId)
{
    m_targets = targets;
    m_primaryTargetId = primaryId;
    update();
}

void MapView::setCrewStart(const QPointF &eastNorth)
{
    m_crewStart = eastNorth;
    m_hasCrewStart = true;
    update();
}

void MapView::setHazardOverlay(const QImage &image, const QRectF &extentMeters)
{
    m_hazardOverlay = image;
    m_hazardExtent = extentMeters;
    update();
}

void MapView::clearHazardOverlay()
{
    m_hazardOverlay = QImage();
    m_hazardExtent = QRectF();
    update();
}

void MapView::setHazardVisible(bool visible)
{
    if (m_hazardVisible == visible)
        return;
    m_hazardVisible = visible;
    update();
}

bool MapView::isHazardVisible() const
{
    return m_hazardVisible;
}

void MapView::setTargetsVisible(bool visible)
{
    if (m_targetsVisible == visible)
        return;
    m_targetsVisible = visible;
    update();
}

bool MapView::areTargetsVisible() const
{
    return m_targetsVisible;
}

const Route &MapView::route() const
{
    return m_route;
}

const QVector<Route> &MapView::planRoutes() const
{
    return m_planRoutes;
}

QVector<RankedTarget> MapView::targets() const
{
    return m_targets;
}

QPointF MapView::crewStart() const
{
    return m_crewStart;
}

void MapView::fitToRoute()
{
    // Include every planned leg so the view frames the whole traverse, not
    // just the immediate primary route.
    QVector<QPointF> allPoints;
    if (m_route.valid)
        allPoints.append(m_route.points);
    for (const Route &leg : m_planRoutes) {
        if (leg.valid)
            allPoints.append(leg.points);
    }
    if (allPoints.isEmpty())
        return;

    double minX = allPoints.first().x();
    double maxX = minX;
    double minY = allPoints.first().y();
    double maxY = minY;
    for (const QPointF &point : allPoints) {
        minX = qMin(minX, point.x());
        maxX = qMax(maxX, point.x());
        minY = qMin(minY, point.y());
        maxY = qMax(maxY, point.y());
    }

    const double spanX =
        qMax((maxX - minX) * (1.0 + kFitRouteMargin), kFitRouteMinSpanM);
    const double spanY =
        qMax((maxY - minY) * (1.0 + kFitRouteMargin), kFitRouteMinSpanM);
    m_center = QPointF((minX + maxX) / 2.0, (minY + maxY) / 2.0);

    if (width() > 0 && height() > 0) {
        m_mpp = clampMpp(qMax(spanX / width(), spanY / height()));
    }
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

QPointF MapView::metersToPixel(const QPointF &meters) const
{
    const QRectF extent = visibleExtentMeters();
    return QPointF((meters.x() - extent.left()) / m_mpp,
                   (extent.bottom() - meters.y()) / m_mpp);
}

void MapView::drawOverlays(QPainter &painter) const
{
    const QRectF visible = visibleExtentMeters();

    // Hazard grid: metre extent -> pixels. readImage-style north-up image
    // (row 0 = north edge), drawn as-is and never flipped.
    if (m_hazardVisible && !m_hazardOverlay.isNull()
        && m_hazardExtent.isValid()
        && m_hazardExtent.width() > 0.0 && m_hazardExtent.height() > 0.0) {
        const QRectF target(
            (m_hazardExtent.left() - visible.left()) / m_mpp,
            (visible.bottom() - m_hazardExtent.bottom()) / m_mpp,
            m_hazardExtent.width() / m_mpp,
            m_hazardExtent.height() / m_mpp);
        painter.save();
        painter.setOpacity(0.30);
        painter.drawImage(target, m_hazardOverlay);
        painter.restore();
    }

    // Planned legs after the primary: dashed lighter cyan with a dark
    // casing, drawn underneath the primary route so the next action reads
    // first.
    for (int i = 1; i < m_planRoutes.size(); ++i) {
        const Route &leg = m_planRoutes.at(i);
        if (!leg.valid || leg.points.size() < 2)
            continue;
        QPolygonF polyline;
        polyline.reserve(leg.points.size());
        for (const QPointF &point : leg.points)
            polyline.append(metersToPixel(point));

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(kRouteCasing, 5, Qt::SolidLine, Qt::RoundCap,
                            Qt::RoundJoin));
        painter.drawPolyline(polyline);
        painter.setPen(QPen(kPlanRouteCore, 3, Qt::DashLine, Qt::RoundCap,
                            Qt::RoundJoin));
        painter.drawPolyline(polyline);
    }

    // Return leg to the haven: dashed amber over a dark casing.
    if (m_returnRoute.valid && m_returnRoute.points.size() >= 2) {
        QPolygonF polyline;
        polyline.reserve(m_returnRoute.points.size());
        for (const QPointF &point : m_returnRoute.points)
            polyline.append(metersToPixel(point));
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(kRouteCasing, 5, Qt::SolidLine, Qt::RoundCap,
                            Qt::RoundJoin));
        painter.drawPolyline(polyline);
        painter.setPen(QPen(QColor(255, 160, 60), 3, Qt::DashDotLine,
                            Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(polyline);
    }

    // Primary route: dark casing under a bright cyan core.
    if (m_route.valid && m_route.points.size() >= 2) {
        QPolygonF polyline;
        polyline.reserve(m_route.points.size());
        for (const QPointF &point : m_route.points)
            polyline.append(metersToPixel(point));

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(kRouteCasing, 6, Qt::SolidLine, Qt::RoundCap,
                            Qt::RoundJoin));
        painter.drawPolyline(polyline);
        painter.setPen(QPen(kRouteCore, 3, Qt::SolidLine, Qt::RoundCap,
                            Qt::RoundJoin));
        painter.drawPolyline(polyline);
    }

    const QRect viewport = rect();

    // Small helper: draw text on a dark translucent backing. Labels are only
    // placed when their marker is inside the viewport, so nothing is drawn
    // completely outside the visible map.
    const auto drawLabel = [&](const QPointF &pixel, const QString &text,
                               const QColor &color) {
        if (!viewport.contains(pixel.toPoint()))
            return;
        const QFontMetrics metrics(painter.font());
        const QRect textBounds = metrics.boundingRect(text);
        const QPoint backing(int(pixel.x()) + kLabelOffsetX,
                             int(pixel.y()) - kLabelOffsetY
                                 - textBounds.height());
        const QRect labelRect(backing.x(), backing.y(),
                              textBounds.width() + 10,
                              textBounds.height() + 6);
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(8, 6, 5, 200));
        painter.drawRect(labelRect);
        painter.setPen(color);
        painter.drawText(labelRect.adjusted(4, 0, -4, 0),
                         Qt::AlignLeft | Qt::AlignVCenter, text);
        painter.restore();
    };

    // Crew start: green dot with dark outline.
    if (m_hasCrewStart) {
        const QPointF pixel = metersToPixel(m_crewStart);
        painter.setPen(QPen(QColor(10, 20, 10, 230), 2));
        painter.setBrush(kCrewGreen);
        painter.drawEllipse(pixel, 7.0, 7.0);
        drawLabel(pixel, QStringLiteral("START"), kLightText);
    }

    // Science targets: amber = reachable, red ring = unreachable/no-go,
    // primary = larger orange marker with its id/name.
    for (const RankedTarget &ranked : m_targets) {
        if (!m_targetsVisible)
            break;
        const QPointF pixel = metersToPixel(ranked.target.position());
        const bool primary = !m_primaryTargetId.isEmpty()
                             && ranked.target.id == m_primaryTargetId;

        if (primary) {
            painter.setPen(QPen(QColor(10, 8, 5, 230), 2));
            painter.setBrush(kPrimaryOrange);
            painter.drawEllipse(pixel, 9.0, 9.0);
            drawLabel(pixel,
                      QStringLiteral("%1 · %2")
                          .arg(ranked.target.id, ranked.target.name),
                      kPrimaryOrange);
        } else if (!ranked.reachable) {
            painter.setPen(QPen(kTargetRed, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(pixel, 6.0, 6.0);
            drawLabel(pixel,
                      QStringLiteral("%1 \u00b7 NO ROUTE")
                          .arg(ranked.target.id),
                      kTargetRed);
        } else if (ranked.walkbackFailed) {
            painter.setPen(QPen(kTargetRed, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(pixel, 6.0, 6.0);
            drawLabel(pixel,
                      QStringLiteral("%1 \u00b7 WALKBACK FAIL")
                          .arg(ranked.target.id),
                      kTargetRed);
        } else {
            painter.setPen(QPen(QColor(10, 8, 5, 230), 2));
            painter.setBrush(kTargetAmber);
            painter.drawEllipse(pixel, 5.0, 5.0);
            drawLabel(pixel, ranked.target.id, kTargetAmber);
        }
    }
}

void MapView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.fillRect(rect(), QColor(24, 18, 15)); // regolith backdrop

    const std::vector<const RasterLayer *> visible =
        orderedVisibleLayers(m_registry, m_visibleLayerIds);

    bool first = true;
    for (const RasterLayer *layer : visible) {
        double opacity = 1.0;
        if (!first) {
            switch (layer->metadata().kind) {
            case RasterLayer::Kind::Hillshade:
                opacity = 0.55;
                break;
            case RasterLayer::Kind::Elevation:
            case RasterLayer::Kind::Slope:
                opacity = 0.50;
                break;
            case RasterLayer::Kind::Optical:
                opacity = 1.0;
                break;
            }
        }
        painter.setOpacity(opacity);
        // readImage is north-up (row 0 = north edge): draw as-is, never flip.
        const QImage img = layer->readImage(visibleExtentMeters(), size());
        if (!img.isNull())
            painter.drawImage(QPoint(0, 0), img);
        first = false;
    }
    painter.setOpacity(1.0);

    // M3 analytic overlays sit above the rasters but below the fixed map
    // chrome (north arrow, scale bar, attribution).
    drawOverlays(painter);

    drawNorthArrow(painter, rect());
    drawScaleBar(painter, rect(), m_mpp);

    if (visible.empty()) {
        // Keep the field-equipment overlays even with no imagery; state the
        // difference between "no data loaded" and "everything hidden".
        painter.setPen(QColor(214, 205, 195));
        painter.drawText(rect(), Qt::AlignCenter,
                         m_registry ? tr("NO LAYERS VISIBLE")
                                    : tr("MAP VIEW — no layers loaded"));
        return;
    }

    // Attribution follows the active layer; when it is hidden or missing,
    // use the topmost visible layer instead.
    const RasterLayer *attribution = activeLayer();
    if (!attribution || !m_visibleLayerIds.contains(attribution->metadata().id))
        attribution = visible.back();
    drawAttribution(painter, rect(), attribution->metadata());
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

void MapView::setReturnRoute(const Route &route)
{
    m_returnRoute = route;
    update();
}

void MapView::clearReturnRoute()
{
    m_returnRoute = Route();
    update();
}

void MapView::clearCrewStart()
{
    m_hasCrewStart = false;
    update();
}

void MapView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const QPointF m = pixelToMeters(event->position());
        emit targetRequested(m.x(), m.y());
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
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
