// M3b traverse-planner tests: hazard-weighted A* on the real shipped slope
// raster and the Mars Trek scenario. Targets and pins are NASA Mars Trek
// coordinates already projected by the scenario; nothing here invents a
// coordinate or changes the 20° no-go threshold.

#include "analysis/HazardMap.h"
#include "analysis/TargetRanking.h"
#include "app/MissionScenario.h"
#include "data/RasterLayer.h"
#include "data/SiteConfig.h"
#include "routing/TraversePlanner.h"

#include <QtTest>

#include <algorithm>
#include <cmath>

namespace
{

constexpr double kStartEasting = -753.397;
constexpr double kStartNorthing = -276224.802;
constexpr double kPahrumpEasting = -756.135;
constexpr double kPahrumpNorthing = -276363.057;

bool buildHazard(HazardMap *hazard, QString *error)
{
    RasterLayer slope(SiteConfig::slopeLayerMetadata());
    const QString path =
        SiteConfig::layerPath(SiteConfig::slopeLayerMetadata().fileName);
    if (path.isEmpty())
    {
        *error = QStringLiteral("asset directory is unknown");
        return false;
    }
    if (!slope.open(path, error))
        return false;
    return hazard->build(slope, HazardMap::kCellSizeM, error);
}

bool loadScenario(MissionScenario *scenario, QString *error)
{
    return MissionScenario::loadFromFile(SiteConfig::scenarioPath(), scenario,
                                         error);
}

}  // namespace

class TestRouting : public QObject
{
    Q_OBJECT

private slots:
    void plansSafeRouteToPahrump();
    void mariasPassIsReachable();
    void noGoTargetStopsAtStandoff();
    void plansLongerRouteToVeraRubin();
    void rejectsDisconnectedStart();
    void routeIsDeterministic();
};

void TestRouting::plansSafeRouteToPahrump()
{
    HazardMap hazard;
    QString error;
    QVERIFY2(buildHazard(&hazard, &error), qPrintable(error));

    const Route route =
        TraversePlanner::plan(hazard, QPointF(kStartEasting, kStartNorthing),
                              QPointF(kPahrumpEasting, kPahrumpNorthing));
    QVERIFY2(route.valid, qPrintable(route.warning));
    QVERIFY(route.distanceM >= 110.0);
    QVERIFY(route.distanceM <= 180.0);
    QVERIFY(route.timeMin >= 4.0);
    QVERIFY(route.timeMin <= 15.0);
    QVERIFY(route.maxSlopeDeg <= HazardMap::kNoGoSlopeDeg + 1e-6);
    QVERIFY(route.standoffM < 5.0);
    QVERIFY2(route.warning.isEmpty(), qPrintable(route.warning));
    QVERIFY(!route.points.isEmpty());

    QVERIFY(hazard.contains(route.points.first().x(),
                            route.points.first().y()));
    QVERIFY(hazard.contains(route.points.last().x(),
                            route.points.last().y()));
    for (const QPointF &point : route.points)
    {
        QVERIFY(hazard.contains(point.x(), point.y()));
        QVERIFY(!hazard.isNoGoAt(point.x(), point.y()));
    }
}

void TestRouting::mariasPassIsReachable()
{
    HazardMap hazard;
    QString error;
    QVERIFY2(buildHazard(&hazard, &error), qPrintable(error));

    MissionScenario scenario;
    QVERIFY2(loadScenario(&scenario, &error), qPrintable(error));
    const ScienceTarget &marias = scenario.targets.at(1);
    QCOMPARE(marias.name, QStringLiteral("Marias Pass"));

    const Route route = TraversePlanner::planToTarget(
        hazard, QPointF(kStartEasting, kStartNorthing), marias);
    QVERIFY2(route.valid, qPrintable(route.warning));
    QVERIFY(route.standoffM < 5.0);
    QVERIFY2(route.warning.isEmpty(), qPrintable(route.warning));
    QVERIFY(route.maxSlopeDeg <= HazardMap::kNoGoSlopeDeg + 1e-6);
    QVERIFY(route.distanceM > 0.0);
    QVERIFY(route.timeMin > 0.0);
}

void TestRouting::noGoTargetStopsAtStandoff()
{
    HazardMap hazard;
    QString error;
    QVERIFY2(buildHazard(&hazard, &error), qPrintable(error));

    // Real Mars Trek "Top of Vera Rubin Ridge" pin; its corrected 4 m slope is
    // 33.92°, above the hard 20° no-go threshold.
    ScienceTarget steep;
    steep.id = QStringLiteral("TEST_STEEP");
    steep.name = QStringLiteral("Top of Vera Rubin Ridge");
    steep.eastingM = -1023.67402622707;
    steep.northingM = -279995.295943866;

    const Route route = TraversePlanner::planToTarget(
        hazard, QPointF(kStartEasting, kStartNorthing), steep);
    QVERIFY2(route.valid, qPrintable(route.warning));
    QVERIFY(!route.points.isEmpty());
    QVERIFY(!hazard.isNoGoAt(route.points.last().x(),
                             route.points.last().y()));
    QVERIFY(route.standoffM > 0.0);
    QVERIFY2(route.warning.contains(steep.name), qPrintable(route.warning));
    QVERIFY2(route.warning.contains(QStringLiteral("standoff"),
                                    Qt::CaseInsensitive),
             qPrintable(route.warning));
}

void TestRouting::plansLongerRouteToVeraRubin()
{
    HazardMap hazard;
    QString error;
    QVERIFY2(buildHazard(&hazard, &error), qPrintable(error));

    MissionScenario scenario;
    QVERIFY2(loadScenario(&scenario, &error), qPrintable(error));
    const ScienceTarget &veraRubin = scenario.targets.at(2);
    QCOMPARE(veraRubin.name, QStringLiteral("Vera Rubin Ridge"));

    const Route pahrump =
        TraversePlanner::plan(hazard, QPointF(kStartEasting, kStartNorthing),
                              QPointF(kPahrumpEasting, kPahrumpNorthing));
    QVERIFY2(pahrump.valid, qPrintable(pahrump.warning));

    const Route route = TraversePlanner::planToTarget(
        hazard, QPointF(kStartEasting, kStartNorthing), veraRubin);
    QVERIFY2(route.valid, qPrintable(route.warning));
    QVERIFY(route.distanceM > pahrump.distanceM);
    QVERIFY(route.timeMin > 0.0);
    // Empty, or a standoff-only note if the pin snapped; never a hard failure.
    QVERIFY(route.warning.isEmpty() ||
            route.warning.contains(QStringLiteral("standoff"),
                                   Qt::CaseInsensitive));
}

void TestRouting::rejectsDisconnectedStart()
{
    HazardMap hazard;
    QString error;
    QVERIFY2(buildHazard(&hazard, &error), qPrintable(error));

    const double farEast =
        std::max(hazard.extentMeters().left(), hazard.extentMeters().right()) +
        50000.0;
    const Route route = TraversePlanner::plan(
        hazard, QPointF(farEast, kStartNorthing),
        QPointF(kPahrumpEasting, kPahrumpNorthing));
    QVERIFY(!route.valid);
    QVERIFY(!route.warning.isEmpty());
    QVERIFY(route.points.isEmpty());
    QCOMPARE(route.distanceM, 0.0);
    QCOMPARE(route.timeMin, 0.0);
}

void TestRouting::routeIsDeterministic()
{
    HazardMap hazard;
    QString error;
    QVERIFY2(buildHazard(&hazard, &error), qPrintable(error));

    const QPointF start(kStartEasting, kStartNorthing);
    const QPointF goal(kPahrumpEasting, kPahrumpNorthing);
    const Route first = TraversePlanner::plan(hazard, start, goal);
    const Route second = TraversePlanner::plan(hazard, start, goal);

    QVERIFY2(first.valid, qPrintable(first.warning));
    QVERIFY2(second.valid, qPrintable(second.warning));
    QCOMPARE(second.points.size(), first.points.size());
    QCOMPARE(second.points.first(), first.points.first());
    QCOMPARE(second.points.last(), first.points.last());
    QCOMPARE(second.distanceM, first.distanceM);
    QCOMPARE(second.timeMin, first.timeMin);
}

QTEST_GUILESS_MAIN(TestRouting)
#include "test_routing.moc"
