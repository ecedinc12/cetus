#pragma once

#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>

class DatasetRegistry;
class RasterLayer;
class QMouseEvent;
class QPaintEvent;
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

    const DatasetRegistry *m_registry = nullptr;  // non-owning
    QString m_activeLayerId;
    QPointF m_center;    // projected metres
    double m_mpp = 1.0;  // metres per pixel
    bool m_panning = false;
    QPoint m_lastMousePos;
    // First show refits the active layer once the widget has its real size;
    // setRegistry() runs in MainWindow's constructor, before layout.
    bool m_didInitialFit = false;
};
