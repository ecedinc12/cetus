// M4-1 walkback tests. The arithmetic tests use hand-computed numbers (shown in
// the comments). The real-data test plans the three Mars Trek targets on the
// shipped slope raster and applies the 360 min budget; the budget, 1.2 return
// factor and 15 min reserve are design choices ([tunable]), not NASA values.

#include "analysis/HazardMap.h"
#include "analysis/TargetRanking.h"
#include "app/MissionScenario.h"
#include "data/RasterLayer.h"
#include "data/SiteConfig.h"
#include "routing/TraversePlanner.h"
#include "routing/Walkback.h"

#include <QtTest>

#include <cmath>
#include <limits>

namespace
{

bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

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

}  // namespace

class TestWalkback : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAreTheDocumentedChoices();
    void nearTargetIsFeasible();
    void farTargetIsRejected();
    void exactLimitIsFeasible();
    void elapsedTimeCountsAgainstBudget();
    void invalidInputsAreRejected();
    void realTargetsAgainstBudget();
};

void TestWalkback::defaultsAreTheDocumentedChoices()
{
    QCOMPARE(Walkback::kTimeBudgetMin, 360.0);
    QCOMPARE(Walkback::kReturnFactor, 1.2);
    QCOMPARE(Walkback::kReserveMin, 15.0);
}

void TestWalkback::nearTargetIsFeasible()
{
    // leg 7.5, work 12, return path 7.5, budget 360, nothing elapsed:
    //   elapsedAfter = 19.5, return = 1.2*7.5 = 9, remaining = 340.5,
    //   margin = 340.5 - 9 - 15 = 316.5
    const WalkbackResult r = Walkback::evaluate(0.0, 7.5, 12.0, 7.5);
    QVERIFY(r.feasible);
    QVERIFY(near(r.elapsedAfterMin, 19.5));
    QVERIFY(near(r.returnMin, 9.0));
    QVERIFY(near(r.reserveMin, 15.0));
    QVERIFY(near(r.remainingAfterMin, 340.5));
    QVERIFY(near(r.marginMin, 316.5));
    QVERIFY(r.reason.isEmpty());
}

void TestWalkback::farTargetIsRejected()
{
    // leg 211.6, work 18, return path 211.6, budget 360:
    //   elapsedAfter = 229.6, return = 253.92, remaining = 130.4,
    //   margin = 130.4 - 253.92 - 15 = -138.52
    const WalkbackResult r = Walkback::evaluate(0.0, 211.6, 18.0, 211.6);
    QVERIFY(!r.feasible);
    QVERIFY(near(r.elapsedAfterMin, 229.6));
    QVERIFY(near(r.returnMin, 253.92));
    QVERIFY(near(r.remainingAfterMin, 130.4));
    QVERIFY(near(r.marginMin, -138.52));
    QVERIFY(!r.reason.isEmpty());
    QVERIFY2(r.reason.contains(QStringLiteral("139")), qPrintable(r.reason));
}

void TestWalkback::exactLimitIsFeasible()
{
    // leg 100, work 20, return path 100:
    //   100 + 20 + 1.2*100 + 15 = 255 min needed.
    QVERIFY(Walkback::evaluate(0.0, 100.0, 20.0, 100.0, 255.0).feasible);
    QVERIFY(!Walkback::evaluate(0.0, 100.0, 20.0, 100.0, 254.9).feasible);
}

void TestWalkback::elapsedTimeCountsAgainstBudget()
{
    // Same near target as above but 200 min already spent:
    //   elapsedAfter = 219.5, remaining = 140.5, margin = 140.5 - 9 - 15 = 116.5
    const WalkbackResult r = Walkback::evaluate(200.0, 7.5, 12.0, 7.5);
    QVERIFY(r.feasible);
    QVERIFY(near(r.elapsedAfterMin, 219.5));
    QVERIFY(near(r.marginMin, 116.5));

    // 340 min already spent: even the near target no longer fits.
    QVERIFY(!Walkback::evaluate(340.0, 7.5, 12.0, 7.5).feasible);
}

void TestWalkback::invalidInputsAreRejected()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!Walkback::evaluate(nan, 7.5, 12.0, 7.5).feasible);
    QVERIFY(!Walkback::evaluate(0.0, -1.0, 12.0, 7.5).feasible);
    QVERIFY(!Walkback::evaluate(0.0, 7.5, 12.0, 7.5, nan).feasible);
    QVERIFY(!Walkback::evaluate(0.0, 7.5, 12.0, 7.5, nan).reason.isEmpty());
}

void TestWalkback::realTargetsAgainstBudget()
{
    HazardMap hazard;
    QString error;
    QVERIFY2(buildHazard(&hazard, &error), qPrintable(error));

    MissionScenario scenario;
    QVERIFY2(MissionScenario::loadFromFile(SiteConfig::scenarioPath(),
                                           &scenario, &error),
             qPrintable(error));
    QVERIFY(scenario.targets.size() == 3);

    // The return path time is taken equal to the outbound path time: the edge
    // cost in TraversePlanner is symmetric, so the reverse route costs the same.
    QHash<QString, WalkbackResult> results;
    for (const ScienceTarget &target : scenario.targets)
    {
        const Route route =
            TraversePlanner::planToTarget(hazard, scenario.crewStart, target);
        QVERIFY2(route.valid, qPrintable(target.id + QStringLiteral(": ") + route.warning));
        const WalkbackResult r = Walkback::evaluate(
            0.0, route.timeMin, target.minutesOnTarget, route.timeMin);
        results.insert(target.id, r);
        qInfo().noquote()
            << target.id << target.name
            << QStringLiteral("travel %1 min, margin %2 min, feasible %3")
                   .arg(route.timeMin, 0, 'f', 1)
                   .arg(r.marginMin, 0, 'f', 1)
                   .arg(r.feasible ? QStringLiteral("yes")
                                   : QStringLiteral("no"));
    }

    QVERIFY2(results.value(QStringLiteral("T1")).feasible,
             "T1 should fit the 360 min budget");
    QVERIFY2(results.value(QStringLiteral("T2")).feasible,
             "T2 should fit the 360 min budget");
    QVERIFY2(!results.value(QStringLiteral("T3")).feasible,
             "T3 should be rejected by the walkback rule at 360 min");
}

QTEST_GUILESS_MAIN(TestWalkback)
#include "test_walkback.moc"
