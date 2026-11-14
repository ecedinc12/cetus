#pragma once

// M5 scripted demo scenario (PLAN.md §6). A fixed sequence of steps S0..S4 so
// the demo is deterministic. Every number here is a DESIGN CHOICE ([tunable]),
// not a NASA value and not a measurement: tau, the shortened EVA budget, the
// comms outage and the power event are scripted scenario events. They are
// labelled as such in the UI banners.
//
// What each step changes in the app:
//   S0  nominal            tau 0.6, full budget, plan as computed
//   S1  dust rising        tau 0.8, warning only, plan unchanged
//   S2  dust storm         tau 1.1, EVA time budget cut; targets that no
//                          longer pass the walkback rule are dropped
//   S3  comms closed       badge "offline until +40 min", no functional change
//   S4  power low          scripted power < 35%: science disabled, return to
//                          the haven (the EVA start point)

#include <QString>

struct ScenarioStep
{
    QString id;                    // "S0".."S4"
    double tau = 0.0;              // dust optical depth shown in the status bar
    double timeBudgetMin = 0.0;    // EVA budget used by the walkback rule
    bool commsClosed = false;
    int commsOfflineMin = 0;       // only meaningful when commsClosed
    bool scienceDisabled = false;  // S4: no science target is recommended
    QString banner;                // empty when nothing needs announcing
};

class ScenarioSteps
{
public:
    // [tunable] storm: tau at which the scenario cuts the EVA budget.
    static constexpr double kStormTau = 1.1;
    // [tunable] storm: shortened EVA time budget, minutes. Chosen so Marias
    // Pass (needs ~108 min incl. walkback) no longer fits while Pahrump Hills
    // (needs ~44 min) still does.
    static constexpr double kStormBudgetMin = 90.0;
    // [tunable] PLAN.md §6 S3: how long the comms window stays closed.
    static constexpr int kCommsOfflineMin = 40;
    // [tunable] PLAN.md §6 S4: scripted power level that forces the return.
    static constexpr double kPowerLowPct = 35.0;

    static int count();
    // Out-of-range indices are clamped to the valid range.
    static const ScenarioStep &at(int index);
};
