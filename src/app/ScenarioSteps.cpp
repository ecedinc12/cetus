#include "app/ScenarioSteps.h"

#include "routing/Walkback.h"

#include <QVector>

#include <algorithm>

namespace
{

QVector<ScenarioStep> buildSteps()
{
    QVector<ScenarioStep> steps;

    ScenarioStep s0;
    s0.id = QStringLiteral("S0");
    s0.tau = 0.6;
    s0.timeBudgetMin = Walkback::kTimeBudgetMin;
    steps.append(s0);

    ScenarioStep s1 = s0;
    s1.id = QStringLiteral("S1");
    s1.tau = 0.8;
    s1.banner = QStringLiteral(
        "Dust rising: tau 0.8 [scenario]. Plan unchanged.");
    steps.append(s1);

    ScenarioStep s2 = s1;
    s2.id = QStringLiteral("S2");
    s2.tau = ScenarioSteps::kStormTau;
    s2.timeBudgetMin = ScenarioSteps::kStormBudgetMin;
    s2.banner =
        QStringLiteral(
            "Dust storm: tau %1 [scenario]. EVA budget cut from %2 to %3 min. "
            "Plan recomputed: targets that fail walkback are dropped.")
            .arg(ScenarioSteps::kStormTau, 0, 'f', 1)
            .arg(Walkback::kTimeBudgetMin, 0, 'f', 0)
            .arg(ScenarioSteps::kStormBudgetMin, 0, 'f', 0);
    steps.append(s2);

    ScenarioStep s3 = s2;
    s3.id = QStringLiteral("S3");
    s3.commsClosed = true;
    s3.commsOfflineMin = ScenarioSteps::kCommsOfflineMin;
    s3.banner =
        QStringLiteral(
            "Comms window closed [scenario]: offline until +%1 min. "
            "No change to the plan.")
            .arg(ScenarioSteps::kCommsOfflineMin);
    steps.append(s3);

    ScenarioStep s4 = s3;
    s4.id = QStringLiteral("S4");
    s4.scienceDisabled = true;
    s4.banner =
        QStringLiteral(
            "Power below %1% [scripted scenario event, not modeled]. Science "
            "disabled: return to the haven (EVA start).")
            .arg(ScenarioSteps::kPowerLowPct, 0, 'f', 0);
    steps.append(s4);

    return steps;
}

const QVector<ScenarioStep> &steps()
{
    static const QVector<ScenarioStep> table = buildSteps();
    return table;
}

}  // namespace

int ScenarioSteps::count()
{
    return static_cast<int>(steps().size());
}

const ScenarioStep &ScenarioSteps::at(int index)
{
    const int last = count() - 1;
    return steps().at(std::clamp(index, 0, last));
}
