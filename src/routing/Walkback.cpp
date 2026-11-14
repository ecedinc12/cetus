#include "routing/Walkback.h"

#include <cmath>

namespace
{
// Floating-point slack so an exactly-on-the-limit plan is not rejected by
// rounding noise (1e-9 minutes is far below anything meaningful).
constexpr double kToleranceMin = 1e-9;
}  // namespace

WalkbackResult Walkback::evaluate(double elapsedMin, double legMin,
                                  double minutesOnTarget,
                                  double returnPathMin, double budgetMin)
{
    WalkbackResult result;

    const double inputs[] = {elapsedMin, legMin, minutesOnTarget,
                             returnPathMin, budgetMin};
    for (const double value : inputs)
    {
        if (!std::isfinite(value) || value < 0.0)
        {
            result.reason = QStringLiteral(
                "walkback cannot be checked: a time input is missing or "
                "negative.");
            return result;
        }
    }

    result.elapsedAfterMin = elapsedMin + legMin + minutesOnTarget;
    result.returnMin = kReturnFactor * returnPathMin;
    result.reserveMin = kReserveMin;
    result.remainingAfterMin = budgetMin - result.elapsedAfterMin;
    result.marginMin =
        result.remainingAfterMin - result.returnMin - result.reserveMin;
    result.feasible = result.marginMin >= -kToleranceMin;

    if (!result.feasible)
    {
        result.reason =
            QStringLiteral(
                "walkback fails: the plan is %1 min over the %2 min budget "
                "(return x%3 plus %4 min reserve).")
                .arg(QString::number(-result.marginMin, 'f', 0),
                     QString::number(budgetMin, 'f', 0),
                     QString::number(kReturnFactor, 'f', 1),
                     QString::number(kReserveMin, 'f', 0));
    }
    return result;
}
