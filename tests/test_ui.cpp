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
#include <QLineF>
#include <QPushButton>
#include <QScrollArea>
#include <QStatusBar>
#include <QTemporaryDir>
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
    void walkbackRejectsFarTarget();
    void advanceScenarioWalksS0ToS4AndWraps();
    void planChainsTargetsWithinBudget();
    void scenarioChangeIsVisibleAndExplained();
    void routeSectionStaysAboveTheFold();
    void mapKeyFitsTheDock();
    void mapOverlaysCanBeHidden();
    void missingAssetsFailLoudly();
    void missionFailureIsLoud();
    void collectsSampleIntoSessionLog();

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
            warning.contains(QStringLiteral("has no data"))) {
            hasExpectedWarning = true;
            break;
        }
    }
    QVERIFY2(hasExpectedWarning, qPrintable(warnings.join(QLatin1Char('|'))));
}

void TestMainWindow::walkbackRejectsFarTarget()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    QVERIFY(w.scenarioLoaded());

    // At the 360 min design budget T1 and T2 fit; T3 has a route but fails
    // the walkback rule, so it must be flagged, never recommended.
    bool sawT3 = false;
    for (const RankedTarget &ranked : w.rankedTargets()) {
        if (ranked.target.id == QStringLiteral("T3")) {
            sawT3 = true;
            QVERIFY(ranked.reachable);
            QVERIFY(ranked.walkbackFailed);
        } else {
            QVERIFY2(!ranked.walkbackFailed,
                     qPrintable(ranked.target.id));
        }
    }
    QVERIFY(sawT3);
    QVERIFY(w.primaryTargetId() != QStringLiteral("T3"));

    SidePanel *panel = w.sidePanel();
    QVERIFY(panel);
    QVERIFY2(panel->walkbackText().contains(QStringLiteral("WITHIN BUDGET")),
             qPrintable(panel->walkbackText()));

    bool hasT3Warning = false;
    for (const QString &warning : panel->warningTexts()) {
        if (warning.startsWith(QStringLiteral("T3")) &&
            warning.contains(QStringLiteral("walkback"),
                             Qt::CaseInsensitive))
            hasT3Warning = true;
    }
    QVERIFY2(hasT3Warning,
             qPrintable(panel->warningTexts().join(QLatin1Char('|'))));

    auto *row = panel->findChild<QLabel *>(QStringLiteral("targetRow_T3"));
    QVERIFY(row);
    QVERIFY2(row->text().contains(QStringLiteral("WALKBACK FAIL")),
             qPrintable(row->text()));
}

void TestMainWindow::advanceScenarioWalksS0ToS4AndWraps()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    QVERIFY(w.scenarioLoaded());
    SidePanel *panel = w.sidePanel();
    QVERIFY(panel);

    const auto failed = [&w](const QString &id) {
        for (const RankedTarget &ranked : w.rankedTargets()) {
            if (ranked.target.id == id)
                return ranked.walkbackFailed;
        }
        return false;
    };

    // S0: nominal, T1 recommended, only T3 fails walkback.
    QCOMPARE(w.scenarioStepIndex(), 0);
    QCOMPARE(w.primaryTargetId(), QStringLiteral("T1"));
    QVERIFY(!failed(QStringLiteral("T2")));
    QVERIFY(failed(QStringLiteral("T3")));

    // S1: dust rising, plan unchanged.
    w.advanceScenario();
    QCOMPARE(w.scenarioStepIndex(), 1);
    QCOMPARE(w.primaryTargetId(), QStringLiteral("T1"));
    QVERIFY(!failed(QStringLiteral("T2")));

    // S2: storm cuts the budget to 90 min; T2 now fails walkback, T1 stays.
    w.advanceScenario();
    QCOMPARE(w.scenarioStepIndex(), 2);
    QCOMPARE(w.missionState().timeBudgetMin, 90.0);
    QCOMPARE(w.missionState().tau, 1.1);
    QCOMPARE(w.primaryTargetId(), QStringLiteral("T1"));
    QVERIFY(!failed(QStringLiteral("T1")));
    QVERIFY(failed(QStringLiteral("T2")));
    QVERIFY(failed(QStringLiteral("T3")));
    QVERIFY(w.route().valid);

    // S3: comms closed, no functional change.
    w.advanceScenario();
    QCOMPARE(w.scenarioStepIndex(), 3);
    QVERIFY(w.missionState().commsClosed);
    QCOMPARE(w.primaryTargetId(), QStringLiteral("T1"));
    QVERIFY2(w.statusBar()->currentMessage().contains(
                 QStringLiteral("COMMS OFFLINE")),
             qPrintable(w.statusBar()->currentMessage()));

    // S4: science disabled. The ROUTE header must say so, not "NO SAFE
    // ROUTE" (a safe route to T1 exists; science is off on purpose).
    w.advanceScenario();
    QCOMPARE(w.scenarioStepIndex(), 4);
    QVERIFY(w.scienceDisabled());
    QVERIFY(w.primaryTargetId().isEmpty());
    QVERIFY(!w.route().valid);
    QVERIFY2(panel->routeSummaryText().contains(
                 QStringLiteral("SCIENCE DISABLED")),
             qPrintable(panel->routeSummaryText()));
    QVERIFY2(!panel->routeSummaryText().contains(
                 QStringLiteral("NO SAFE ROUTE")),
             qPrintable(panel->routeSummaryText()));

    // One more press wraps back to S0 and restores the nominal plan.
    w.advanceScenario();
    QCOMPARE(w.scenarioStepIndex(), 0);
    QVERIFY(!w.scienceDisabled());
    QCOMPARE(w.primaryTargetId(), QStringLiteral("T1"));
    QVERIFY(w.route().valid);
    QVERIFY(!panel->routeSummaryText().contains(
        QStringLiteral("SCIENCE DISABLED")));
}

void TestMainWindow::planChainsTargetsWithinBudget()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    QVERIFY(w.scenarioLoaded());
    QCOMPARE(w.missionState().elapsedMin, 0.0);

    // S0 (360 min): T1 first (it is the primary target), then T2 chained
    // from T1. T3 is too far to leave room for the walk back.
    QStringList expected = {QStringLiteral("T1"), QStringLiteral("T2")};
    QCOMPARE(w.missionPlan().ids(), expected);
    QCOMPARE(w.missionPlan().steps.first().targetId, w.primaryTargetId());
    QVERIFY(w.missionPlan().elapsedAfterMin <= 360.0);
    QVERIFY(w.missionPlan().marginMin >= 0.0);
    // Elapsed time only grows along the chain.
    QVERIFY(w.missionPlan().steps.at(1).elapsedAfterMin >
            w.missionPlan().steps.at(0).elapsedAfterMin);
    SidePanel *panel = w.sidePanel();
    QVERIFY(panel);
    auto *consumables =
        panel->findChild<QLabel *>(QStringLiteral("consumablesLabel"));
    QVERIFY(consumables);
    QVERIFY2(consumables->text().contains(QStringLiteral("PLAN: T1")),
             qPrintable(consumables->text()));
    QVERIFY2(consumables->text().contains(QStringLiteral("T2")),
             qPrintable(consumables->text()));

    auto *map = w.findChild<MapView *>();
    QVERIFY(map);
    QCOMPARE(map->planRoutes().size(), 2);

    // S2 (90 min): T2 no longer fits after T1 (needs > 90 min whatever the
    // leg is), so the plan is T1 alone.
    w.advanceScenario();
    w.advanceScenario();
    QCOMPARE(w.scenarioStepIndex(), 2);
    expected = {QStringLiteral("T1")};
    QCOMPARE(w.missionPlan().ids(), expected);
    QVERIFY(w.missionPlan().elapsedAfterMin <= 90.0);
    QCOMPARE(map->planRoutes().size(), 1);

    // S4: science disabled, no plan.
    w.advanceScenario();
    w.advanceScenario();
    QCOMPARE(w.scenarioStepIndex(), 4);
    QVERIFY(w.missionPlan().steps.isEmpty());
    QVERIFY(map->planRoutes().isEmpty());
}

void TestMainWindow::scenarioChangeIsVisibleAndExplained()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    QVERIFY(w.scenarioLoaded());
    SidePanel *panel = w.sidePanel();
    QVERIFY(panel);
    auto *map = w.findChild<MapView *>();
    QVERIFY(map);

    const auto context = [panel]() {
        return panel->scenarioContextText();
    };
    const auto targetPosition = [&w](const QString &id) {
        QPointF position;
        for (const RankedTarget &ranked : w.rankedTargets()) {
            if (ranked.target.id == id)
                position = ranked.target.position();
        }
        return position;
    };

    // S0: nominal, full T1 -> T2 plan.
    QVERIFY(panel->findChild<QLabel *>(
        QStringLiteral("scenarioContextLabel")));
    QVERIFY2(context().contains(QStringLiteral("S0")), qPrintable(context()));
    QVERIFY2(context().contains(QStringLiteral("NOMINAL")),
             qPrintable(context()));
    QVERIFY2(context().contains(QStringLiteral("Full plan")),
             qPrintable(context()));
    QVERIFY2(context().contains(QStringLiteral("T1")), qPrintable(context()));
    QVERIFY2(context().contains(QStringLiteral("T2")), qPrintable(context()));
    QCOMPARE(map->planRoutes().size(), 2);

    // S1: dust rising, plan unchanged.
    w.advanceScenario();
    QVERIFY2(context().contains(QStringLiteral("DUST RISING")),
             qPrintable(context()));
    QVERIFY2(context().contains(QStringLiteral("Plan unchanged")),
             qPrintable(context()));
    QCOMPARE(map->planRoutes().size(), 2);

    // S2: the storm cuts the budget and drops T2 from the plan.
    w.advanceScenario();
    QCOMPARE(w.scenarioStepIndex(), 2);
    QVERIFY2(context().contains(QStringLiteral("DUST STORM")),
             qPrintable(context()));
    QVERIFY2(context().contains(QStringLiteral("360")), qPrintable(context()));
    QVERIFY2(context().contains(QStringLiteral("90")), qPrintable(context()));
    QVERIFY2(context().contains(QStringLiteral("plan now T1")),
             qPrintable(context()));
    QCOMPARE(map->planRoutes().size(), 1);
    QVERIFY(w.route().valid);
    QVERIFY(!w.route().points.isEmpty());
    // The route ends at the safe snap point next to T1, not the pin itself.
    QVERIFY(QLineF(w.route().points.constLast(),
                   targetPosition(QStringLiteral("T1")))
                .length()
            < 10.0);

    // S3: comms outage only; the plan is unchanged.
    w.advanceScenario();
    QVERIFY2(context().contains(QStringLiteral("COMMS CLOSED")),
             qPrintable(context()));
    QVERIFY2(context().contains(QStringLiteral("offline")),
             qPrintable(context()));
    QVERIFY2(context().contains(QStringLiteral("plan unchanged")),
             qPrintable(context()));
    QCOMPARE(map->planRoutes().size(), 1);

    // S4: science disabled, route and plan absent.
    w.advanceScenario();
    QVERIFY2(context().contains(QStringLiteral("POWER LOW")),
             qPrintable(context()));
    QVERIFY2(context().contains(QStringLiteral("science disabled")),
             qPrintable(context()));
    QVERIFY(map->planRoutes().isEmpty());
    QVERIFY(!w.route().valid);
}

void TestMainWindow::routeSectionStaysAboveTheFold()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    w.resize(1280, 800);
    w.show();
    QTest::qWait(20);

    // Move to the storm step, where warnings were previously longest.
    w.advanceScenario();
    w.advanceScenario();
    QCOMPARE(w.scenarioStepIndex(), 2);
    QCoreApplication::processEvents();
    QTest::qWait(20);

    SidePanel *panel = w.sidePanel();
    QVERIFY(panel);
    auto *routeLabel =
        panel->findChild<QLabel *>(QStringLiteral("routeSummaryLabel"));
    QVERIFY(routeLabel);
    auto *scroll =
        panel->findChild<QScrollArea *>(QStringLiteral("sidePanelScroll"));
    QVERIFY(scroll);

    const QPoint routeTop =
        routeLabel->mapTo(scroll->viewport(), QPoint(0, 0));
    QVERIFY2(routeTop.y() < scroll->viewport()->height(),
             qPrintable(QStringLiteral("route header y %1, viewport height %2")
                            .arg(routeTop.y())
                            .arg(scroll->viewport()->height())));

    auto *walkback =
        panel->findChild<QLabel *>(QStringLiteral("walkbackNote"));
    QVERIFY(walkback);
    const QPoint walkTop =
        walkback->mapTo(scroll->viewport(), QPoint(0, 0));
    QVERIFY2(walkTop.y() >= 0 &&
                 walkTop.y() + walkback->height() <= scroll->viewport()->height(),
             qPrintable(QStringLiteral("walkback y %1 height %2, viewport height %3")
                            .arg(walkTop.y())
                            .arg(walkback->height())
                            .arg(scroll->viewport()->height())));

    // The T2 warning is the compact walkback line, not the old banner.
    bool sawCompactT2 = false;
    const QString banner = ScenarioSteps::at(2).banner;
    for (const QString &warning : panel->warningTexts()) {
        if (warning.startsWith(QStringLiteral("T2")) &&
            warning.contains(QStringLiteral("walkback fails by"))) {
            sawCompactT2 = true;
            QVERIFY(warning != banner);
        }
    }
    QVERIFY2(sawCompactT2,
             qPrintable(panel->warningTexts().join(QLatin1Char('|'))));

    w.close();
}

void TestMainWindow::mapKeyFitsTheDock()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    w.resize(1280, 800);
    w.show();
    QCoreApplication::processEvents();

    QLabel *key = w.findChild<QLabel *>(QStringLiteral("mapKeyNote"));
    QVERIFY(key);
    QVERIFY2(key->text().contains(QStringLiteral("DATA NOTE")),
             qPrintable(key->text()));

    // The rendered height must cover the wrapped text...
    QVERIFY2(key->height() >= key->heightForWidth(key->width()),
             qPrintable(QStringLiteral("key %1x%2, heightForWidth(%3) = %4")
                            .arg(key->width())
                            .arg(key->height())
                            .arg(key->width())
                            .arg(key->heightForWidth(key->width()))));
    // ...and the last line must sit inside the window.
    const int bottom = key->mapTo(&w, QPoint(0, key->height())).y();
    QVERIFY2(bottom <= w.height(),
             qPrintable(QStringLiteral("key bottom %1, window height %2")
                            .arg(bottom)
                            .arg(w.height())));

    w.close();
}

void TestMainWindow::mapOverlaysCanBeHidden()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    MainWindow w;
    w.resize(960, 700);
    w.show();
    QTest::qWait(20);

    auto *map = w.findChild<MapView *>();
    QVERIFY(map);
    auto *hazard = w.findChild<QToolButton *>(QStringLiteral("hazardButton"));
    auto *targets =
        w.findChild<QToolButton *>(QStringLiteral("targetsButton"));
    QVERIFY(hazard);
    QVERIFY(targets);

    // Both overlays are on by default (hazard-first).
    QVERIFY(hazard->isCheckable());
    QVERIFY(hazard->isChecked());
    QVERIFY(targets->isCheckable());
    QVERIFY(targets->isChecked());
    QVERIFY(map->isHazardVisible());
    QVERIFY(map->areTargetsVisible());

    // Hiding the hazard grid changes what the map paints and nothing else.
    QImage hazardOn(map->size(), QImage::Format_ARGB32_Premultiplied);
    hazardOn.fill(QColor(0, 0, 0));
    map->render(&hazardOn);

    hazard->click();
    QVERIFY(!map->isHazardVisible());
    QVERIFY(map->areTargetsVisible());
    QImage hazardOff(map->size(), QImage::Format_ARGB32_Premultiplied);
    hazardOff.fill(QColor(0, 0, 0));
    map->render(&hazardOff);
    QVERIFY(hazardOn != hazardOff);

    hazard->click();
    QVERIFY(map->isHazardVisible());

    // Targets: toggling does not touch the plan or the route.
    const QString primaryBefore = w.primaryTargetId();
    targets->click();
    QVERIFY(!map->areTargetsVisible());
    QCOMPARE(w.primaryTargetId(), primaryBefore);
    QVERIFY(w.route().valid);
    targets->click();
    QVERIFY(map->areTargetsVisible());

    w.close();
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


void TestMainWindow::collectsSampleIntoSessionLog()
{
    if (!hasPreparedAssets())
        QSKIP("prepared HiRISE ortho not present");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString logPath = dir.filePath(QStringLiteral("samples.jsonl"));

    const bool hadLog = qEnvironmentVariableIsSet("CETUS_SAMPLE_LOG");
    const QByteArray previousLog = qgetenv("CETUS_SAMPLE_LOG");
    qputenv("CETUS_SAMPLE_LOG", logPath.toUtf8());

    const auto logLines = [&logPath]() {
        QFile file(logPath);
        if (!file.open(QIODevice::ReadOnly))
            return 0;
        int count = 0;
        while (!file.atEnd()) {
            if (!file.readLine().trimmed().isEmpty())
                ++count;
        }
        return count;
    };

    {
        MainWindow w;
        QVERIFY(w.scenarioLoaded());
        QCOMPARE(w.sampleCount(), 0);

        auto *button = w.findChild<QPushButton *>(
            QStringLiteral("collectSampleButton"));
        QVERIFY(button);
        QVERIFY(button->isEnabled());
        QVERIFY2(button->text().contains(QStringLiteral("T1")),
                 qPrintable(button->text()));

        // The always-visible toolbar twin must be usable too: at the demo
        // window size the panel button can sit below the fold.
        auto *toolbarButton = w.findChild<QToolButton *>(
            QStringLiteral("collectSampleToolbarButton"));
        QVERIFY(toolbarButton);
        QVERIFY(toolbarButton->isEnabled());

        QTest::mouseClick(toolbarButton, Qt::LeftButton);
        QCOMPARE(w.sampleCount(), 1);
        QVERIFY(w.sidePanel());
        QCOMPARE(w.sidePanel()->latestSampleId(),
                 QStringLiteral("SAMPLE-3-1"));

        auto *latest = w.sidePanel()->findChild<QLabel *>(
            QStringLiteral("latestSampleLabel"));
        QVERIFY(latest);
        QVERIFY2(latest->text().contains(QStringLiteral("E ")),
                 qPrintable(latest->text()));
        QVERIFY2(latest->text().contains(QStringLiteral("N ")),
                 qPrintable(latest->text()));
        QVERIFY2(latest->text().contains(QStringLiteral("up (")),
                 qPrintable(latest->text()));
        QVERIFY2(latest->text().contains(QStringLiteral("slope ")),
                 qPrintable(latest->text()));
        QVERIFY2(latest->text().contains(QStringLiteral("SAMPLE-3-1")),
                 qPrintable(latest->text()));

        QVERIFY2(w.statusBar()->currentMessage().contains(
                     QStringLiteral("SAMPLE-3-1 LOGGED")),
                 qPrintable(w.statusBar()->currentMessage()));
        QCOMPARE(logLines(), 1);
    }

    {
        MainWindow w;
        QVERIFY(w.scenarioLoaded());
        QCOMPARE(w.sampleCount(), 1);

        auto *button = w.findChild<QPushButton *>(
            QStringLiteral("collectSampleButton"));
        QVERIFY(button);
        QVERIFY(button->isEnabled());

        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(w.sampleCount(), 2);
        QVERIFY(w.sidePanel());
        QCOMPARE(w.sidePanel()->latestSampleId(),
                 QStringLiteral("SAMPLE-3-2"));
        QCOMPARE(logLines(), 2);
    }

    if (hadLog)
        qputenv("CETUS_SAMPLE_LOG", previousLog);
    else
        qunsetenv("CETUS_SAMPLE_LOG");
}

QTEST_MAIN(TestMainWindow)
#include "test_ui.moc"
