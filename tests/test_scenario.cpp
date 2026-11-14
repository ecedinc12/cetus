// M5 scripted scenario table: pure data, no rasters or GUI. The numbers are
// design choices ([tunable]); these tests pin the table and show why the storm
// budget was chosen using hand-computed walkback arithmetic.

#include "app/ScenarioSteps.h"
#include "routing/Walkback.h"

#include <QtTest>

class TestScenario : public QObject
{
    Q_OBJECT

private slots:
    void tableHasFiveOrderedSteps();
    void stepsCarryTheDocumentedValues();
    void outOfRangeIndexIsClamped();
    void stormBudgetDropsMariasPassButKeepsPahrump();
};

void TestScenario::tableHasFiveOrderedSteps()
{
    QVERIFY(ScenarioSteps::count() == 5);
    const char *ids[] = {"S0", "S1", "S2", "S3", "S4"};
    for (int i = 0; i < 5; ++i)
        QCOMPARE(ScenarioSteps::at(i).id, QString::fromLatin1(ids[i]));
}

void TestScenario::stepsCarryTheDocumentedValues()
{
    const double tau[] = {0.6, 0.8, 1.1, 1.1, 1.1};
    const double budget[] = {360.0, 360.0, 90.0, 90.0, 90.0};
    for (int i = 0; i < 5; ++i) {
        QCOMPARE(ScenarioSteps::at(i).tau, tau[i]);
        QCOMPARE(ScenarioSteps::at(i).timeBudgetMin, budget[i]);
    }

    // Only S3 and S4 have the comms window closed; only S4 disables science.
    for (int i = 0; i < 5; ++i) {
        QCOMPARE(ScenarioSteps::at(i).commsClosed, i >= 3);
        QCOMPARE(ScenarioSteps::at(i).scienceDisabled, i == 4);
    }
    QCOMPARE(ScenarioSteps::at(3).commsOfflineMin, 40);

    // S0 is nominal and silent; every later step announces itself.
    QVERIFY(ScenarioSteps::at(0).banner.isEmpty());
    for (int i = 1; i < 5; ++i)
        QVERIFY2(!ScenarioSteps::at(i).banner.isEmpty(), qPrintable(QString::number(i)));
    QVERIFY(ScenarioSteps::at(2).banner.contains(QStringLiteral("90")));
    QVERIFY(ScenarioSteps::at(2).banner.contains(QStringLiteral("360")));
}

void TestScenario::outOfRangeIndexIsClamped()
{
    QCOMPARE(ScenarioSteps::at(-5).id, QStringLiteral("S0"));
    QCOMPARE(ScenarioSteps::at(99).id, QStringLiteral("S4"));
}

void TestScenario::stormBudgetDropsMariasPassButKeepsPahrump()
{
    // Pahrump-like: leg 7.5, work 12, return 7.5 -> needs 7.5+12+9+15 = 43.5.
    // Marias-like: leg 36, work 14, return 36 -> needs 36+14+43.2+15 = 108.2.
    const double storm = ScenarioSteps::kStormBudgetMin;
    QVERIFY(Walkback::evaluate(0.0, 7.5, 12.0, 7.5, storm).feasible);
    QVERIFY(!Walkback::evaluate(0.0, 36.0, 14.0, 36.0, storm).feasible);
    // With the nominal budget both fit.
    QVERIFY(Walkback::evaluate(0.0, 36.0, 14.0, 36.0).feasible);
}

QTEST_GUILESS_MAIN(TestScenario)
#include "test_scenario.moc"
