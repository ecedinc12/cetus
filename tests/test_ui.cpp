// MainWindow wiring tests: prepared-layer load, legend wiring, multi-layer
// toggling, fit/render, and loud failure on a missing asset dir. Runs
// offscreen (QT_QPA_PLATFORM=offscreen set by CTest).

#include "data/DatasetRegistry.h"
#include "ui/LayerLegend.h"
#include "ui/MainWindow.h"
#include "ui/MapView.h"
#include "ui/SidePanel.h"

#include <QCheckBox>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QStatusBar>
#include <QToolButton>
#include <QtTest/QTest>

class TestMainWindow : public QObject
{
    Q_OBJECT
private slots:
    void loadsPreparedLayers();
    void fitsRouteOnFirstShow();
    void legendTogglesLayer();
    void fitAndRender();
    void plansMissionOnStartup();
    void panelExplainsTheRecommendation();
    void missingAssetsFailLoudly();
    void missionFailureIsLoud();

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

    LayerLegend *legend = w.legend();
    QVERIFY(legend);
    QCOMPARE(legend->layerCount(), 4);

    auto *map = w.findChild<MapView *>();
    QVERIFY(map);
    QCOMPARE(map->activeLayerId(), QStringLiteral("hirise_optical"));
    QCOMPARE(map->visibleLayerIds(),
             QStringList{QStringLiteral("hirise_optical")});

    QCheckBox *optical = legend->checkbox(QStringLiteral("hirise_optical"));
    QVERIFY(optical);
    QVERIFY(optical->isChecked());

    QLabel *slopeDetail = legend->detailLabel(QStringLiteral("hirise_slope"));
    QVERIFY(slopeDetail);
    QVERIFY(slopeDetail->text().contains(QStringLiteral("HiRISE DTM / MRO")));
    QVERIFY(slopeDetail->text().contains(QStringLiteral("degrees")));
}

void TestMainWindow::fitsRouteOnFirstShow()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    w.resize(960, 700);
    w.show();
    QTest::qWait(20);

    auto *map = w.findChild<MapView *>();
    QVERIFY(map);
    QVERIFY(map->visibleLayerIds()
            == QStringList{QStringLiteral("hirise_optical")});

    QVERIFY(w.scenarioLoaded());
    QVERIFY(w.route().valid);
    QVERIFY(!w.route().points.isEmpty());

    const QRectF visible = map->visibleExtentMeters();
    // The post-layout fit is fitToRoute(): every route point must be visible
    // and the view keeps enough surrounding terrain to orient the crew.
    for (const QPointF &point : w.route().points)
        QVERIFY2(visible.contains(point), "route point outside visible extent");
    QVERIFY(visible.width() >= 400.0);
    QVERIFY(visible.height() >= 400.0);

    w.close();
}

void TestMainWindow::legendTogglesLayer()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    w.resize(640, 480);
    w.show();
    QTest::qWait(20);

    auto *map = w.findChild<MapView *>();
    QVERIFY(map);
    LayerLegend *legend = w.legend();
    QVERIFY(legend);

    QImage imageBefore(map->size(), QImage::Format_ARGB32_Premultiplied);
    imageBefore.fill(QColor(0, 0, 0));
    map->render(&imageBefore);

    QCheckBox *slope = legend->checkbox(QStringLiteral("hirise_slope"));
    QVERIFY(slope);
    QVERIFY(!slope->isChecked());
    QVERIFY(!map->isLayerVisible(QStringLiteral("hirise_slope")));

    slope->setChecked(true);
    QCoreApplication::processEvents();
    QVERIFY(map->isLayerVisible(QStringLiteral("hirise_slope")));

    QImage imageAfter(map->size(), QImage::Format_ARGB32_Premultiplied);
    imageAfter.fill(QColor(0, 0, 0));
    map->render(&imageAfter);
    QVERIFY(imageBefore != imageAfter);

    slope->setChecked(false);
    QCoreApplication::processEvents();
    QVERIFY(!map->isLayerVisible(QStringLiteral("hirise_slope")));

    w.close();
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

void TestMainWindow::plansMissionOnStartup()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    QVERIFY(w.scenarioLoaded());
    QCOMPARE(w.scenario().targets.size(), 3);
    QCOMPARE(w.rankedTargets().size(), 3);
    // Value-rate arithmetic picks T1 (Pahrump Hills): high science value and
    // the shortest safe travel time.
    QCOMPARE(w.primaryTargetId(), QStringLiteral("T1"));
    QVERIFY(w.route().valid);
    QVERIFY(w.route().distanceM >= 110.0);
    QVERIFY(w.route().distanceM <= 180.0);

    SidePanel *panel = w.sidePanel();
    QVERIFY(panel);
    QCOMPARE(panel->primaryTargetId(), QStringLiteral("T1"));
    QCOMPARE(panel->rankedTargetCount(), 3);

    auto *map = w.findChild<MapView *>();
    QVERIFY(map);
    QCOMPARE(map->targets().size(), 3);
    QVERIFY(map->route().valid);
}

void TestMainWindow::panelExplainsTheRecommendation()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    QVERIFY(w.scenarioLoaded());
    SidePanel *panel = w.sidePanel();
    QVERIFY(panel);

    const QString goal = panel->scienceGoalText();
    QVERIFY2(goal.contains(QStringLiteral("Mount Sharp")), qPrintable(goal));
    QVERIFY2(goal.contains(QStringLiteral("fluid")), qPrintable(goal));

    const QString why = panel->primaryTargetWhy();
    QVERIFY2(why.contains(QStringLiteral("Jarosite")), qPrintable(why));

    const QString summary = panel->routeSummaryText();
    const QString distance =
        QString::number(w.route().distanceM, 'f', 0);
    QVERIFY2(summary.contains(distance), qPrintable(summary));

    // A route warning and the unknown-data warning are both legitimately
    // optional, so require at least one of them rather than a specific line.
    const QStringList warnings = panel->warningTexts();
    bool hasExpectedWarning = false;
    for (const QString &warning : warnings) {
        if ((!w.route().warning.isEmpty() &&
             warning.contains(w.route().warning)) ||
            warning.contains(QStringLiteral("unknown (no data)"))) {
            hasExpectedWarning = true;
            break;
        }
    }
    QVERIFY2(hasExpectedWarning, qPrintable(warnings.join(QLatin1Char('|'))));
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
        QVERIFY(w.statusBar()->currentMessage().startsWith(
            QStringLiteral("RASTER LOAD FAILED:")));
        LayerLegend *legend = w.legend();
        QVERIFY(legend);
        QCOMPARE(legend->layerCount(), 0);
    }

    if (hadAssetDir)
        qputenv("CETUS_ASSET_DIR", previous);
    else
        qunsetenv("CETUS_ASSET_DIR");
}

void TestMainWindow::missionFailureIsLoud()
{
    const bool hadAssetDir = qEnvironmentVariableIsSet("CETUS_ASSET_DIR");
    const QByteArray previous = qgetenv("CETUS_ASSET_DIR");
    qputenv("CETUS_ASSET_DIR", QByteArray("/nonexistent"));

    {
        MainWindow w;
        QVERIFY(!w.loadError().isEmpty());
        QVERIFY(!w.scenarioLoaded());
        QVERIFY(w.primaryTargetId().isEmpty());
        QVERIFY(!w.route().valid);
        QVERIFY(w.statusBar()->currentMessage().startsWith(
            QStringLiteral("RASTER LOAD FAILED:")));
    }

    if (hadAssetDir)
        qputenv("CETUS_ASSET_DIR", previous);
    else
        qunsetenv("CETUS_ASSET_DIR");
}

QTEST_MAIN(TestMainWindow)
#include "test_ui.moc"
