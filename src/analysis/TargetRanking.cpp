#include "analysis/TargetRanking.h"

#include <algorithm>
#include <cmath>

double TargetRanking::valueRate(double scienceValue, double travelMinutes,
                                double minutesOnTarget)
{
    const double denominator = travelMinutes + minutesOnTarget;
    if (!std::isfinite(denominator) || denominator <= 0.0 ||
        !std::isfinite(scienceValue))
        return 0.0;
    return scienceValue / denominator;
}

QVector<RankedTarget> TargetRanking::rank(
    const QVector<ScienceTarget> &targets,
    const QHash<QString, double> &travelMinutesById)
{
    QVector<RankedTarget> reachable;
    QVector<RankedTarget> unreachable;
    reachable.reserve(targets.size());

    for (const ScienceTarget &target : targets)
    {
        RankedTarget ranked;
        ranked.target = target;

        const auto it = travelMinutesById.constFind(target.id);
        const double travel =
            it == travelMinutesById.constEnd() ? -1.0 : it.value();
        if (!std::isfinite(travel) || travel < 0.0)
        {
            ranked.reachable = false;
            ranked.travelMinutes = 0.0;
            ranked.valueRate = 0.0;
            unreachable.push_back(ranked);
            continue;
        }

        ranked.reachable = true;
        ranked.travelMinutes = travel;
        ranked.valueRate =
            valueRate(target.scienceValue, travel, target.minutesOnTarget);
        reachable.push_back(ranked);
    }

    std::stable_sort(reachable.begin(), reachable.end(),
                     [](const RankedTarget &a, const RankedTarget &b) {
                         if (a.valueRate != b.valueRate)
                             return a.valueRate > b.valueRate;
                         if (a.travelMinutes != b.travelMinutes)
                             return a.travelMinutes < b.travelMinutes;
                         return a.target.id < b.target.id;
                     });

    std::stable_sort(unreachable.begin(), unreachable.end(),
                     [](const RankedTarget &a, const RankedTarget &b) {
                         return a.target.id < b.target.id;
                     });

    reachable += unreachable;
    return reachable;
}
