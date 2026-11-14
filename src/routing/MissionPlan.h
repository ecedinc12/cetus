#pragma once

// Greedy multi-target plan (PLAN.md §5.5 steps 3-4). Starting at the EVA start
// point, repeatedly pick the reachable target with the best value rate
// (value / (leg + time on target)) whose visit still satisfies the walkback
// rule from the crew's current position and elapsed time, then continue from
// that target. Stops when no remaining target passes walkback.
//
// Pure arithmetic: leg times are supplied by the caller (TraversePlanner), so
// this module never touches rasters. Walkback haven = EVA start (design
// choice); the return time from a target is its time from the start, because
// the traverse edge cost is symmetric.
//
// No optional 2-opt pass (PLAN.md §5.5 step 5): not implemented.

#include "analysis/TargetRanking.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

struct PlanStep
{
    QString targetId;
    double legMin = 0.0;            // walk from the previous stop (or start)
    double minutesOnTarget = 0.0;
    double elapsedAfterMin = 0.0;   // EVA clock after finishing this target
    double returnMin = 0.0;         // Walkback::kReturnFactor x path to haven
    double marginMin = 0.0;         // budget left after return + reserve
};

struct MissionPlan
{
    QVector<PlanStep> steps;
    double elapsedAfterMin = 0.0;   // EVA clock after the last step
    double marginMin = 0.0;         // margin after the last step (0 if empty)

    QStringList ids() const;
    // "T1 -> T2" with a real arrow, or "none" for an empty plan.
    QString summary() const;

    // fromStartMin: travel time start -> target id; negative means
    //               unreachable (such a target is never planned).
    // legMin:       travel time between two targets; negative means no route
    //               (that hop is skipped).
    // elapsedMin:   EVA time already spent.
    static MissionPlan build(
        const QVector<ScienceTarget> &targets,
        const QHash<QString, double> &fromStartMin,
        const std::function<double(const QString &, const QString &)> &legMin,
        double elapsedMin, double budgetMin);
};
