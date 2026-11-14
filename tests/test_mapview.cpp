// M1b MapView canvas tests: empty render, real-layer fit, zoom clamps,
// pan direction and cursor-anchored wheel zoom. Runs offscreen
// (QT_QPA_PLATFORM=offscreen set by CTest).

#include "analysis/TargetRanking.h"
#include "data/DatasetRegistry.h"
#include "data/RasterLayer.h"
#include "routing/TraversePlanner.h"
#include "ui/MapView.h"

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QSignalSpy>
#include <QWheelEvent>
#include <QtTest/QTest>

#include <cmath>

class TestMapView : public QObject
{
    Q_OBJECT
private slots:
    void emptyViewDoesNotCrash();
    void loadRealLayers();
    void realRasterRendersPixels();
    void fitShowsWholeLayer();
    void zoomChangesScale();
    void panMovesCentre();
    void wheelZoomKeepsCursorPoint();
    void layerTogglesAffectRendering();
    void soloActiveHidesOthers();
    void hideAllLayersLeavesEmptyView();
    void unknownLayerIdIsIgnored();
    void drawsRouteAndTargets();
    void drawsHazardOverlay();
    void fitToRouteShowsContext();

private:
    static QString orthoPath();
    static bool hasPreparedAssets();
    static int nonBackgroundPixels(const QImage &img);
};

// Canvas background used by MapView::paintEvent(); pixels that differ from it
// mean the raster (or another layer) actually painted.
static const QColor kCanvasBackground(24, 18, 15);
static const int kBackgroundTolerance = 12;

int TestMapView::nonBackgroundPixels(const QImage &img)
{
    int count = 0;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb *row = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb px = row[x];
            if (qAbs(qRed(px) - kCanvasBackground.red()) > kBackgroundTolerance
                || qAbs(qGreen(px) - kCanvasBackground.green()) > kBackgroundTolerance
                || qAbs(qBlue(px) - kCanvasBackground.blue()) > kBackgroundTolerance) {
                ++count;
            }
        }
    }
    return count;
}

QString TestMapView::orthoPath()
{
    const QString dir =
        qEnvironmentVariable("CETUS_ASSET_DIR", QStringLiteral("assets"));
    return dir + QStringLiteral("/layers/gale_hirise_ortho_1m.tif");
}

bool TestMapView::hasPreparedAssets()
{
    return QFile::exists(orthoPath());
}

void TestMapView::emptyViewDoesNotCrash()
{
    MapView view;
    view.resize(320, 240);
    QImage img(view.size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(0, 0, 0));
    view.render(&img);
    QVERIFY(view.metersPerPixel() > 0.0);
    QVERIFY(view.activeLayerId().isEmpty());
}

void TestMapView::loadRealLayers()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    DatasetRegistry reg;
    QString err;
    QVERIFY2(reg.loadPrepared(&err), qPrintable(err));

    MapView view;
    view.setRegistry(&reg);
    view.resize(640, 480);
    view.fitToScene();

    QCOMPARE(view.activeLayerId(), QStringLiteral("hirise_optical"));
    QVERIFY(view.metersPerPixel() >= 0.25);
    QVERIFY(view.metersPerPixel() <= 200.0);

    const RasterLayer *ortho = reg.layer(QStringLiteral("hirise_optical"));
    QVERIFY(ortho != nullptr);
    QVERIFY(view.visibleExtentMeters().intersects(ortho->info().extent));
}

void TestMapView::realRasterRendersPixels()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    DatasetRegistry reg;
    QString err;
    QVERIFY2(reg.loadPrepared(&err), qPrintable(err));

    MapView view;
    view.setRegistry(&reg);
    view.resize(640, 480);
    view.fitToScene();

    QImage img(view.size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(0, 0, 0));
    view.render(&img);

    // Overlays (north arrow, scale bar, attribution) are small; a rendered
    // real layer must cover at least a fifth of the canvas.
    const int nonBackground = nonBackgroundPixels(img);
    QVERIFY2(nonBackground > img.width() * img.height() / 5,
             qPrintable(QStringLiteral("only %1 non-background pixels")
                            .arg(nonBackground)));
    QVERIFY(view.metersPerPixel() > 0.0);
}

void TestMapView::fitShowsWholeLayer()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    DatasetRegistry reg;
    QString err;
    QVERIFY2(reg.loadPrepared(&err), qPrintable(err));

    MapView view;
    view.setRegistry(&reg);
    view.resize(640, 480);
    view.fitToScene();

    const RasterLayer *layer = reg.layer(view.activeLayerId());
    QVERIFY(layer != nullptr);
    const QRectF extent = layer->info().extent;
    const QRectF visible = view.visibleExtentMeters();

    // 1% slack on each edge.
    const double tolX = extent.width() * 0.01;
    const double tolY = extent.height() * 0.01;
    QVERIFY(visible.left() <= extent.left() + tolX);
    QVERIFY(visible.right() >= extent.right() - tolX);
    QVERIFY(visible.top() <= extent.top() + tolY);
    QVERIFY(visible.bottom() >= extent.bottom() - tolY);
}

void TestMapView::zoomChangesScale()
{
    MapView view;
    const double original = view.metersPerPixel();
    QVERIFY(original > 0.0);

    view.zoomIn();
    QVERIFY(view.metersPerPixel() < original);

    view.zoomOut();
    view.zoomOut();
    QVERIFY(view.metersPerPixel() > original);
}

void TestMapView::panMovesCentre()
{
    MapView view;
    view.resize(400, 300);
    view.setMetersPerPixel(2.0);
    const QPointF oldCenter = view.visibleExtentMeters().center();

    QSignalSpy viewSpy(&view, &MapView::viewChanged);
    QSignalSpy cursorSpy(&view, &MapView::cursorMoved);

    QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, QPoint(200, 150));
    QTest::mouseMove(&view, QPoint(240, 150));
    QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier,
                        QPoint(240, 150));

    const QPointF newCenter = view.visibleExtentMeters().center();
    // Drag right moves the map east, so the centre x decreases.
    QVERIFY(newCenter.x() < oldCenter.x());
    // Drag was horizontal: y must not drift.
    QVERIFY(qAbs(newCenter.y() - oldCenter.y()) < 1e-9);

    QVERIFY(viewSpy.count() >= 1);
    QVERIFY(cursorSpy.count() >= 1);
    // cursorMoved carries the projected position of the cursor.
    const QList<QVariant> args = cursorSpy.first();
    QVERIFY(args.size() == 2);
    QVERIFY(std::isfinite(args.at(0).toDouble()));
    QVERIFY(std::isfinite(args.at(1).toDouble()));
}

void TestMapView::wheelZoomKeepsCursorPoint()
{
    MapView view;
    view.resize(400, 300);
    view.setMetersPerPixel(4.0);
    const QPointF cursorPx(120.0, 90.0);

    const auto pointAtCursor = [&view, &cursorPx]() {
        const QRectF extent = view.visibleExtentMeters();
        return QPointF(extent.left() + cursorPx.x() * view.metersPerPixel(),
                       extent.bottom() - cursorPx.y() * view.metersPerPixel());
    };

    const QPointF before = pointAtCursor();
    QWheelEvent wheel(cursorPx, cursorPx, QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&view, &wheel);
    const QPointF after = pointAtCursor();

    QVERIFY(view.metersPerPixel() < 4.0);  // wheel up zooms in
    QVERIFY(qAbs(after.x() - before.x()) < 1e-6);
    QVERIFY(qAbs(after.y() - before.y()) < 1e-6);
}

void TestMapView::layerTogglesAffectRendering()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    DatasetRegistry reg;
    QString err;
    QVERIFY2(reg.loadPrepared(&err), qPrintable(err));

    MapView view;
    view.setRegistry(&reg);
    view.resize(480, 360);
    view.fitToScene();

    const auto render = [&view]() {
        QImage img(view.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(0, 0, 0));
        view.render(&img);
        return img;
    };

    const QImage imageA = render();
    view.setLayerVisible(QStringLiteral("hirise_slope"), true);

    const QStringList ids = view.visibleLayerIds();
    QVERIFY(ids.contains(QStringLiteral("hirise_optical")));
    QVERIFY(ids.contains(QStringLiteral("hirise_slope")));
    QVERIFY(ids.indexOf(QStringLiteral("hirise_optical"))
            < ids.indexOf(QStringLiteral("hirise_slope")));

    const QImage imageB = render();
    QVERIFY(imageA != imageB);
}

void TestMapView::soloActiveHidesOthers()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    DatasetRegistry reg;
    QString err;
    QVERIFY2(reg.loadPrepared(&err), qPrintable(err));

    MapView view;
    view.setRegistry(&reg);
    view.resize(480, 360);
    view.setActiveLayer(QStringLiteral("hirise_slope"));

    QCOMPARE(view.activeLayerId(), QStringLiteral("hirise_slope"));
    QCOMPARE(view.visibleLayerIds(), QStringList{QStringLiteral("hirise_slope")});
    QVERIFY(view.isLayerVisible(QStringLiteral("hirise_slope")));
    QVERIFY(!view.isLayerVisible(QStringLiteral("hirise_optical")));
}

void TestMapView::hideAllLayersLeavesEmptyView()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    DatasetRegistry reg;
    QString err;
    QVERIFY2(reg.loadPrepared(&err), qPrintable(err));

    MapView view;
    view.setRegistry(&reg);
    view.resize(480, 360);
    view.fitToScene();

    view.hideAllLayers();
    QVERIFY(view.visibleLayerIds().isEmpty());

    QImage img(view.size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(0, 0, 0));
    view.render(&img);
    QVERIFY(!img.isNull());

    view.setLayerVisible(QStringLiteral("hirise_slope"), true);
    QCOMPARE(view.visibleLayerIds(), QStringList{QStringLiteral("hirise_slope")});
}

void TestMapView::unknownLayerIdIsIgnored()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    DatasetRegistry reg;
    QString err;
    QVERIFY2(reg.loadPrepared(&err), qPrintable(err));

    MapView view;
    view.setRegistry(&reg);
    view.resize(480, 360);
    view.fitToScene();

    const QStringList before = view.visibleLayerIds();
    view.setLayerVisible(QStringLiteral("no_such_layer"), true);
    QCOMPARE(view.visibleLayerIds(), before);
    QVERIFY(!view.isLayerVisible(QStringLiteral("no_such_layer")));

    QImage img(view.size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(0, 0, 0));
    view.render(&img);
    QVERIFY(!img.isNull());
}

void TestMapView::drawsRouteAndTargets()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    DatasetRegistry reg;
    QString err;
    QVERIFY2(reg.loadPrepared(&err), qPrintable(err));

    MapView view;
    view.setRegistry(&reg);
    view.resize(640, 480);
    view.fitToScene();

    const QRectF extent = view.visibleExtentMeters();
    const QPointF start = extent.center();
    const QPointF end = start + QPointF(100.0, 0.0);

    Route route;
    route.points = {start, end};
    route.distanceM = 100.0;
    route.timeMin = 2.0;
    route.maxSlopeDeg = 4.0;
    route.valid = true;

    RankedTarget ranked;
    ranked.target.id = QStringLiteral("T1");
    ranked.target.name = QStringLiteral("Test Vein");
    ranked.target.eastingM = end.x();
    ranked.target.northingM = end.y();
    ranked.reachable = true;
    ranked.travelMinutes = 2.0;

    const auto render = [&view]() {
        QImage img(view.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(0, 0, 0));
        view.render(&img);
        return img;
    };

    const QImage before = render();
    view.setRoute(route);
    view.setTargets({ranked}, QStringLiteral("T1"));
    view.setCrewStart(start);
    const QImage after = render();

    QVERIFY(before != after);
    QCOMPARE(view.route().distanceM, 100.0);
    QCOMPARE(view.targets().size(), 1);
    QCOMPARE(view.crewStart(), start);
}

void TestMapView::drawsHazardOverlay()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    DatasetRegistry reg;
    QString err;
    QVERIFY2(reg.loadPrepared(&err), qPrintable(err));

    MapView view;
    view.setRegistry(&reg);
    view.resize(640, 480);
    view.fitToScene();

    const QRectF extent = view.visibleExtentMeters();
    const QRectF overlayExtent(
        extent.center().x() - extent.width() / 4.0,
        extent.center().y() - extent.height() / 4.0,
        extent.width() / 2.0, extent.height() / 2.0);

    QImage hazard(8, 8, QImage::Format_ARGB32_Premultiplied);
    hazard.fill(QColor(255, 0, 0, 255));

    const auto render = [&view]() {
        QImage img(view.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(0, 0, 0));
        view.render(&img);
        return img;
    };

    const QImage before = render();
    view.setHazardOverlay(hazard, overlayExtent);
    const QImage after = render();

    QVERIFY(before != after);
}

void TestMapView::fitToRouteShowsContext()
{
    MapView view;
    view.resize(640, 480);
    view.setMetersPerPixel(0.5);

    const QPointF start(1000.0, 2000.0);
    const QPointF end(1100.0, 2000.0);
    Route route;
    route.points = {start, end};
    route.distanceM = 100.0;
    route.timeMin = 2.0;
    route.valid = true;
    view.setRoute(route);

    view.fitToRoute();

    const QRectF visible = view.visibleExtentMeters();
    QVERIFY2(visible.width() >= 400.0,
             qPrintable(QStringLiteral("visible width %1 m")
                            .arg(visible.width())));
    QVERIFY(visible.contains(start));
    QVERIFY(visible.contains(end));
}

QTEST_MAIN(TestMapView)
#include "test_mapview.moc"
