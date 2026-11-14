// M1c MainWindow wiring tests: prepared-layer load, toolbar layer switching,
// fit/render, and loud failure on a missing asset dir. Runs offscreen
// (QT_QPA_PLATFORM=offscreen set by CTest).

#include "data/DatasetRegistry.h"
#include "ui/MainWindow.h"
#include "ui/MapView.h"

#include <QComboBox>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QToolButton>
#include <QtTest/QTest>

class TestMainWindow : public QObject
{
    Q_OBJECT
private slots:
    void loadsPreparedLayers();
    void fitsRasterOnFirstShow();
    void comboSwitchesLayer();
    void fitAndRender();
    void missingAssetsFailLoudly();

private:
    static QString orthoPath();
    static bool hasPreparedAssets();
};

QString TestMainWindow::orthoPath()
{
    const QString dir =
        qEnvironmentVariable("CETUS_ASSET_DIR", QStringLiteral("assets"));
    return dir + QStringLiteral("/layers/gale_hirise_ortho_1m.tif");
}

bool TestMainWindow::hasPreparedAssets()
{
    return QFile::exists(orthoPath());
}

void TestMainWindow::loadsPreparedLayers()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    QVERIFY(w.layersLoaded());
    QCOMPARE(w.registry().layers().size(), size_t(4));
    QCOMPARE(w.loadError(), QString());

    auto *combo = w.findChild<QComboBox *>(QStringLiteral("layerCombo"));
    QVERIFY(combo);
    QCOMPARE(combo->count(), 4);

    auto *map = w.findChild<MapView *>();
    QVERIFY(map);
    QCOMPARE(map->activeLayerId(), QStringLiteral("hirise_optical"));
}

void TestMainWindow::fitsRasterOnFirstShow()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    w.resize(960, 700);
    w.show();
    QTest::qWait(20);

    auto *map = w.findChild<MapView *>();
    QVERIFY(map);

    const RasterLayer *layer = w.registry().layer(map->activeLayerId());
    QVERIFY(layer);
    const QRectF layerExtent = layer->info().extent;
    QVERIFY(layerExtent.width() > 0.0 && layerExtent.height() > 0.0);

    const QRectF visible = map->visibleExtentMeters();
    // First show must fit against the real (960x700) widget size, so the
    // layer extent lies inside the view (1% slack) and the view is not much
    // larger than the layer. A fit against the pre-show default widget size
    // fails both bounds.
    const double tolX = layerExtent.width() * 0.01;
    const double tolY = layerExtent.height() * 0.01;
    QVERIFY(visible.contains(layerExtent.center()));
    QVERIFY(visible.left() <= layerExtent.left() + tolX);
    QVERIFY(visible.right() >= layerExtent.right() - tolX);
    QVERIFY(visible.top() <= layerExtent.top() + tolY);
    QVERIFY(visible.bottom() >= layerExtent.bottom() - tolY);
    // Fit is limited by the tighter axis, so the other axis may show extra
    // ground (aspect mismatch). Allow 3x per axis; a fit against the pre-show
    // default widget size (mpp clamped at 200) is far larger than that.
    QVERIFY(visible.width() <= 3.0 * layerExtent.width());
    QVERIFY(visible.height() <= 3.0 * layerExtent.height());

    w.close();
}

void TestMainWindow::comboSwitchesLayer()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    auto *combo = w.findChild<QComboBox *>(QStringLiteral("layerCombo"));
    QVERIFY(combo);
    auto *map = w.findChild<MapView *>();
    QVERIFY(map);

    const int idx = combo->findData(QStringLiteral("hirise_slope"));
    QVERIFY(idx >= 0);
    combo->setCurrentIndex(idx);
    QCoreApplication::processEvents();

    QCOMPARE(map->activeLayerId(), QStringLiteral("hirise_slope"));
}

void TestMainWindow::fitAndRender()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    auto *fit = w.findChild<QToolButton *>(QStringLiteral("fitButton"));
    QVERIFY(fit);
    QTest::mouseClick(fit, Qt::LeftButton);

    w.resize(640, 480);
    QCoreApplication::processEvents();

    auto *map = w.findChild<MapView *>();
    QVERIFY(map);
    QVERIFY(map->metersPerPixel() > 0.0);

    QImage image(map->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(0, 0, 0));
    map->render(&image);
    QVERIFY(!image.isNull());
}

void TestMainWindow::missingAssetsFailLoudly()
{
    const bool hadAssetDir = qEnvironmentVariableIsSet("CETUS_ASSET_DIR");
    const QByteArray previous = qgetenv("CETUS_ASSET_DIR");
    qputenv("CETUS_ASSET_DIR", QByteArray("/nonexistent"));

    {
        MainWindow w;
        QVERIFY(!w.layersLoaded());
        QVERIFY(!w.loadError().isEmpty());
        auto *combo = w.findChild<QComboBox *>(QStringLiteral("layerCombo"));
        QVERIFY(combo);
        QCOMPARE(combo->count(), 0);
    }

    if (hadAssetDir)
        qputenv("CETUS_ASSET_DIR", previous);
    else
        qunsetenv("CETUS_ASSET_DIR");
}

QTEST_MAIN(TestMainWindow)
#include "test_ui.moc"
