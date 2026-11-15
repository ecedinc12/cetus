#include "routing/MissionPlan.h"

#include "routing/Walkback.h"

#include <cmath>

QStringList MissionPlan::ids() const
{
    QStringList list;
    list.reserve(steps.size());
    for (const PlanStep &step : steps)
        list.append(step.targetId);
    return list;
}

QString MissionPlan::summary() const
{
    if (steps.isEmpty())
        return QStringLiteral("none");
    return ids().join(QStringLiteral(" \u2192 "));
}

MissionPlan MissionPlan::build(
    const QVector<ScienceTarget> &targets,
    const QHash<QString, double> &fromStartMin,
    const std::function<double(const QString &, const QString &)> &legMin,
    double elapsedMin, double budgetMin, bool oneWay)
{
    MissionPlan plan;
    plan.elapsedAfterMin = elapsedMin;

    // Only targets with a route from the start can ever be planned.
    QVector<ScienceTarget> remaining;
    for (const ScienceTarget &target : targets) {
        const double fromStart = fromStartMin.value(target.id, -1.0);
        if (std::isfinite(fromStart) && fromStart >= 0.0)
            remaining.append(target);
    }

    QString currentId;  // empty = the EVA start point
    double elapsed = elapsedMin;

    while (!remaining.isEmpty()) {
        int bestIndex = -1;
        double bestRate = 0.0;
        double bestLeg = 0.0;
        WalkbackResult bestCheck;

        for (int i = 0; i < remaining.size(); ++i) {
            const ScienceTarget &candidate = remaining.at(i);

            double leg = -1.0;
            if (currentId.isEmpty())
                leg = fromStartMin.value(candidate.id, -1.0);
            else if (legMin)
                leg = legMin(currentId, candidate.id);
            if (!std::isfinite(leg) || leg < 0.0)
                continue;  // no known route for this hop

            // Return path = candidate -> start = start -> candidate time.
            const WalkbackResult check = Walkback::evaluate(
                elapsed, leg, candidate.minutesOnTarget,
                oneWay ? 0.0 : fromStartMin.value(candidate.id), budgetMin);
            if (!check.feasible)
                continue;

            const double rate = TargetRanking::valueRate(
                candidate.scienceValue, leg, candidate.minutesOnTarget);
            // Strict '>' keeps the earlier target on a tie (deterministic).
            if (bestIndex < 0 || rate > bestRate + 1e-12) {
                bestIndex = i;
                bestRate = rate;
                bestLeg = leg;
                bestCheck = check;
            }
        }

        if (bestIndex < 0)
            break;  // nothing left passes walkback

        const ScienceTarget &chosen = remaining.at(bestIndex);
        PlanStep step;
        step.targetId = chosen.id;
        step.legMin = bestLeg;
        step.minutesOnTarget = chosen.minutesOnTarget;
        step.elapsedAfterMin = bestCheck.elapsedAfterMin;
        step.returnMin = bestCheck.returnMin;
        step.marginMin = bestCheck.marginMin;
        plan.steps.append(step);

        elapsed = bestCheck.elapsedAfterMin;
        plan.elapsedAfterMin = elapsed;
        plan.marginMin = bestCheck.marginMin;
        currentId = chosen.id;
        remaining.removeAt(bestIndex);
    }

    return plan;
}
