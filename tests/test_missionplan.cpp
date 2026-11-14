// Greedy multi-target plan: pure arithmetic with hand-built leg times (no
// rasters). Expected numbers are computed by hand in the comments.
// Walkback rule: elapsed + leg + onTarget + 1.2 * return + 15 <= budget.

#include "analysis/TargetRanking.h"
#include "routing/MissionPlan.h"

#include <QHash>
#include <QStringList>
#include <QtTest/QTest>

namespace {

ScienceTarget makeTarget(const QString &id, double value, double onMin)
{
    ScienceTarget target;
    target.id = id;
    target.name = id;
    target.scienceValue = value;
    target.minutesOnTarget = onMin;
    return target;
}

QString pairKey(const QString &a, const QString &b)
{
    return a < b ? a + QLatin1Char('|') + b : b + QLatin1Char('|') + a;
}

struct Legs
{
    QHash<QString, double> pair;
    double operator()(const QString &a, const QString &b) const
    {
        return pair.value(pairKey(a, b), -1.0);
    }
};

}  // namespace

class TestMissionPlan : public QObject
{
    Q_OBJECT
private slots:
    void emptyInputGivesEmptyPlan();
    void picksBestRateThenChains();
    void walkbackStopsTheChain();
    void skipsUnreachableTargets();
    void respectsElapsedTime();
    void nextLegIsMeasuredFromCurrentPosition();
    void summaryUsesArrows();
};

void TestMissionPlan::emptyInputGivesEmptyPlan()
{
    const MissionPlan plan =
        MissionPlan::build({}, {}, Legs{}, 0.0, 360.0);
    QVERIFY(plan.steps.isEmpty());
    QCOMPARE(plan.summary(), QStringLiteral("none"));
    QCOMPARE(plan.elapsedAfterMin, 0.0);
}

// A: value 0.9, 10 min on target; B: value 0.5, 10 min. Both 10 min from the
// start, A<->B 5 min. Rates from start: A 0.9/20 = 0.045, B 0.5/20 = 0.025.
// Step A: elapsed 20, return 1.2*10 = 12, margin 360-20-12-15 = 313.
// Step B (from A): elapsed 20+5+10 = 35, return 12, margin 360-35-12-15 = 298.
void TestMissionPlan::picksBestRateThenChains()
{
    const QVector<ScienceTarget> targets = {
        makeTarget(QStringLiteral("B"), 0.5, 10.0),
        makeTarget(QStringLiteral("A"), 0.9, 10.0),
    };
    const QHash<QString, double> fromStart = {
        {QStringLiteral("A"), 10.0}, {QStringLiteral("B"), 10.0}};
    Legs legs;
    legs.pair.insert(pairKey(QStringLiteral("A"), QStringLiteral("B")), 5.0);

    const MissionPlan plan =
        MissionPlan::build(targets, fromStart, legs, 0.0, 360.0);

    QCOMPARE(plan.steps.size(), 2);
    QCOMPARE(plan.steps.at(0).targetId, QStringLiteral("A"));
    QCOMPARE(plan.steps.at(0).legMin, 10.0);
    QCOMPARE(plan.steps.at(0).elapsedAfterMin, 20.0);
    QCOMPARE(plan.steps.at(0).returnMin, 12.0);
    QCOMPARE(plan.steps.at(0).marginMin, 313.0);
    QCOMPARE(plan.steps.at(1).targetId, QStringLiteral("B"));
    QCOMPARE(plan.steps.at(1).legMin, 5.0);
    QCOMPARE(plan.steps.at(1).elapsedAfterMin, 35.0);
    QCOMPARE(plan.steps.at(1).marginMin, 298.0);
    QCOMPARE(plan.elapsedAfterMin, 35.0);
    QCOMPARE(plan.marginMin, 298.0);
}

// Same data, budget 60. A: margin 60-20-12-15 = 13 (fits). B: 60-35-12-15 =
// -2, rejected, so the plan stops after A.
void TestMissionPlan::walkbackStopsTheChain()
{
    const QVector<ScienceTarget> targets = {
        makeTarget(QStringLiteral("A"), 0.9, 10.0),
        makeTarget(QStringLiteral("B"), 0.5, 10.0),
    };
    const QHash<QString, double> fromStart = {
        {QStringLiteral("A"), 10.0}, {QStringLiteral("B"), 10.0}};
    Legs legs;
    legs.pair.insert(pairKey(QStringLiteral("A"), QStringLiteral("B")), 5.0);

    const MissionPlan plan =
        MissionPlan::build(targets, fromStart, legs, 0.0, 60.0);

    QCOMPARE(plan.ids(), QStringList{QStringLiteral("A")});
    QCOMPARE(plan.marginMin, 13.0);
}

void TestMissionPlan::skipsUnreachableTargets()
{
    const QVector<ScienceTarget> targets = {
        makeTarget(QStringLiteral("A"), 0.9, 10.0),
        makeTarget(QStringLiteral("B"), 0.5, 10.0),
    };

    // B has no route from the start: never planned.
    QHash<QString, double> fromStart = {
        {QStringLiteral("A"), 10.0}, {QStringLiteral("B"), -1.0}};
    Legs legs;
    legs.pair.insert(pairKey(QStringLiteral("A"), QStringLiteral("B")), 5.0);
    MissionPlan plan = MissionPlan::build(targets, fromStart, legs, 0.0, 360.0);
    QCOMPARE(plan.ids(), QStringList{QStringLiteral("A")});

    // B is reachable from the start but there is no known A -> B leg: the
    // greedy chain only continues over known legs, so it stops after A.
    fromStart.insert(QStringLiteral("B"), 10.0);
    plan = MissionPlan::build(targets, fromStart, Legs{}, 0.0, 360.0);
    QCOMPARE(plan.ids(), QStringList{QStringLiteral("A")});
}

// Elapsed 300, budget 360, same targets. A: elapsed 320, margin 360-320-12-15
// = 13 (fits). B after A: elapsed 335, margin -2 (rejected). Elapsed 340: A
// would end at 360 and has no room for the walk back, so the plan is empty.
void TestMissionPlan::respectsElapsedTime()
{
    const QVector<ScienceTarget> targets = {
        makeTarget(QStringLiteral("A"), 0.9, 10.0),
        makeTarget(QStringLiteral("B"), 0.5, 10.0),
    };
    const QHash<QString, double> fromStart = {
        {QStringLiteral("A"), 10.0}, {QStringLiteral("B"), 10.0}};
    Legs legs;
    legs.pair.insert(pairKey(QStringLiteral("A"), QStringLiteral("B")), 5.0);

    MissionPlan plan =
        MissionPlan::build(targets, fromStart, legs, 300.0, 360.0);
    QCOMPARE(plan.ids(), QStringList{QStringLiteral("A")});
    QCOMPARE(plan.elapsedAfterMin, 320.0);
    QCOMPARE(plan.marginMin, 13.0);

    plan = MissionPlan::build(targets, fromStart, legs, 340.0, 360.0);
    QVERIFY(plan.steps.isEmpty());
    QCOMPARE(plan.elapsedAfterMin, 340.0);
}

// A(0.9), B(0.6), C(0.6), all 10 min on target and 10 min from the start.
// Legs: A-B 50, A-C 2, B-C 3. From the start A wins (0.045 vs 0.03). After A
// the rates are B 0.6/60 = 0.01 and C 0.6/12 = 0.05, so C goes next although
// B and C look identical from the start. Then B: elapsed 20, 32, 45; margin
// at the end 360-45-12-15 = 288.
void TestMissionPlan::nextLegIsMeasuredFromCurrentPosition()
{
    const QVector<ScienceTarget> targets = {
        makeTarget(QStringLiteral("A"), 0.9, 10.0),
        makeTarget(QStringLiteral("B"), 0.6, 10.0),
        makeTarget(QStringLiteral("C"), 0.6, 10.0),
    };
    const QHash<QString, double> fromStart = {{QStringLiteral("A"), 10.0},
                                              {QStringLiteral("B"), 10.0},
                                              {QStringLiteral("C"), 10.0}};
    Legs legs;
    legs.pair.insert(pairKey(QStringLiteral("A"), QStringLiteral("B")), 50.0);
    legs.pair.insert(pairKey(QStringLiteral("A"), QStringLiteral("C")), 2.0);
    legs.pair.insert(pairKey(QStringLiteral("B"), QStringLiteral("C")), 3.0);

    const MissionPlan plan =
        MissionPlan::build(targets, fromStart, legs, 0.0, 360.0);

    const QStringList expected = {QStringLiteral("A"), QStringLiteral("C"),
                                  QStringLiteral("B")};
    QCOMPARE(plan.ids(), expected);
    QCOMPARE(plan.elapsedAfterMin, 45.0);
    QCOMPARE(plan.marginMin, 288.0);
}

void TestMissionPlan::summaryUsesArrows()
{
    MissionPlan plan;
    PlanStep first;
    first.targetId = QStringLiteral("T1");
    PlanStep second;
    second.targetId = QStringLiteral("T2");
    plan.steps = {first, second};
    QCOMPARE(plan.summary(), QStringLiteral("T1 \u2192 T2"));
}

QTEST_MAIN(TestMissionPlan)
#include "test_missionplan.moc"
