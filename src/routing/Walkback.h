#pragma once

// Walkback feasibility (PLAN.md §5.4). The crew must always be able to walk
// back to the safe haven before the EVA time budget runs out:
//
//   elapsed + leg + time_on_target + 1.2 * return_path + 15 min reserve
//       <= time_budget
//
// This module is pure arithmetic: it takes times in minutes and never touches
// rasters. Callers supply the walking times from TraversePlanner.
//
// Everything below is a DESIGN CHOICE ([tunable]), not a NASA value.

#include <QString>

struct WalkbackResult
{
    bool feasible = false;
    double elapsedAfterMin = 0.0;    // elapsed + leg + time on target
    double returnMin = 0.0;          // kReturnFactor * return path time
    double reserveMin = 0.0;         // kReserveMin
    double remainingAfterMin = 0.0;  // budget - elapsedAfterMin (can be < 0)
    double marginMin = 0.0;          // remaining - return - reserve
    QString reason;                  // empty when feasible
};

class Walkback
{
public:
    // [tunable] total EVA time budget, minutes. Chosen by the user (360 min)
    // so the far target is rejected by the rule in the demo.
    static constexpr double kTimeBudgetMin = 360.0;
    // [tunable] PLAN.md §5.4: the walk back is assumed 1.2x slower than the
    // planned path time (tired crew, depleted consumables).
    static constexpr double kReturnFactor = 1.2;
    // [tunable] PLAN.md §5.4: reserve that must remain after returning.
    static constexpr double kReserveMin = 15.0;

    // elapsedMin       time already spent on the EVA
    // legMin           walking time from the crew's position to the target
    // minutesOnTarget  work time at the target
    // returnPathMin    planned (unscaled) walking time target -> safe haven
    // Negative or non-finite inputs are rejected (feasible = false).
    static WalkbackResult evaluate(double elapsedMin, double legMin,
                                   double minutesOnTarget,
                                   double returnPathMin,
                                   double budgetMin = kTimeBudgetMin);
};
