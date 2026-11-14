#pragma once

#include "analysis/TargetRanking.h"
#include "routing/TraversePlanner.h"

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class DatasetRegistry;
class RasterLayer;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QShowEvent;
class QWheelEvent;

// M1b map canvas: shows the active raster layer inside a view defined by a
// centre in projected metres plus metres-per-pixel. Supports pan, wheel zoom
// about the cursor, scale bar / north arrow / attribution overlays.
// Raster reads go through RasterLayer only (AGENTS.md §6).
class MapView : public QWidget
{
    Q_OBJECT
public:
    explicit MapView(QWidget *parent = nullptr);

    // Non-owning. Passing nullptr clears the view.
    void setRegistry(const DatasetRegistry *registry);
    void setActiveLayer(const QString &layerId);
    QString activeLayerId() const;

    // M2 multi-layer visibility. Layers draw bottom-to-top by Kind; the
    // active layer remains the attribution/selection anchor.
    void setLayerVisible(const QString &layerId, bool visible);
    bool isLayerVisible(const QString &layerId) const;
    // IDs in bottom-to-top draw order.
    QStringList visibleLayerIds() const;
    void showAllLayers();
    void hideAllLayers();

    // M3 overlays: hazard grid, planned route, crew start and science
    // targets. These are display-only; routing/ranking lives elsewhere.
    void setRoute(const Route &route);
    void clearRoute();
    // Whole multi-target plan in order: the primary leg first, then the
    // target-to-target legs. Drawn underneath the primary route.
    void setPlanRoutes(const QVector<Route> &routes);
    void clearPlanRoutes();
    void setTargets(const QVector<RankedTarget> &targets, const QString &primaryId);
    void setCrewStart(const QPointF &eastNorth);
    void setHazardOverlay(const QImage &image, const QRectF &extentMeters);
    void clearHazardOverlay();
    void fitToRoute();

    // Display toggles for the analytic overlays (default: visible). Hiding
    // the hazard grid or the target markers does not change the plan; the
    // route legs and the crew start stay drawn.
    void setHazardVisible(bool visible);
    bool isHazardVisible() const;
    void setTargetsVisible(bool visible);
    bool areTargetsVisible() const;

    const Route &route() const;
    const QVector<Route> &planRoutes() const;
    QVector<RankedTarget> targets() const;
    QPointF crewStart() const;

    void fitToScene();
    void setMetersPerPixel(double metersPerPixel);
    double metersPerPixel() const;

    // x = west, y = south, width/height = metres. y grows north, so
    // top() is the south edge and bottom() is the north edge.
    QRectF visibleExtentMeters() const;

    QSize sizeHint() const override;

public slots:
    void zoomIn();
    void zoomOut();

signals:
    void cursorMoved(double eastingM, double northingM);
    void viewChanged();
    void visibleLayersChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    // Active layer lookup, or nullptr when no registry / no / unknown id.
    const RasterLayer *activeLayer() const;
    // Pixel -> projected metres using the current centre and mpp
    // (east = left + x*mpp, north = bottom - y*mpp).
    QPointF pixelToMeters(const QPointF &pixel) const;
    // Projected metres -> widget pixels (inverse of pixelToMeters).
    QPointF metersToPixel(const QPointF &meters) const;
    void drawOverlays(QPainter &painter) const;

    const DatasetRegistry *m_registry = nullptr;  // non-owning
    QString m_activeLayerId;
    QStringList m_visibleLayerIds;
    QPointF m_center;    // projected metres
    double m_mpp = 1.0;  // metres per pixel
    bool m_panning = false;
    QPoint m_lastMousePos;
    // First show refits the active layer once the widget has its real size;
    // setRegistry() runs in MainWindow's constructor, before layout.
    bool m_didInitialFit = false;

    Route m_route;
    QVector<Route> m_planRoutes;
    QVector<RankedTarget> m_targets;
    QString m_primaryTargetId;
    QPointF m_crewStart;
    bool m_hasCrewStart = false;
    QImage m_hazardOverlay;
    QRectF m_hazardExtent;
    bool m_hazardVisible = true;
    bool m_targetsVisible = true;
};
