#pragma once

#include "routing/Walkback.h"

#include <QTime>

// Plain mission-state snapshot (AGENTS.md §6: one object, passed explicitly).
// Values below are the demo scenario S0 (PLAN.md §6). The scripted scenario
// steps (app/ScenarioSteps.h) overwrite tau, the time budget and the comms flag.
struct MissionState
{
    int sol = 3;                 // Sol number of the current EVA.
    QTime clock = QTime(8, 40);  // Local Mars solar time on the surface.
    double tau = 0.6;            // Dust optical depth (dimensionless).

    // EVA time budget used by the walkback rule, minutes ([tunable] design
    // choice; see Walkback::kTimeBudgetMin).
    double timeBudgetMin = Walkback::kTimeBudgetMin;
    // EVA time already spent, minutes. The crew position is not simulated, so
    // the demo keeps this at 0 (the plan starts at the EVA start point); the
    // walkback rule and the greedy plan both honour a non-zero value.
    double elapsedMin = 0.0;
    bool commsClosed = false;
    int commsOfflineMin = 0;

    // Consumables, percent remaining (suit / rover margins). NOT shown in the
    // UI: there is no endurance data behind these numbers, they are
    // placeholders only.
    double o2Pct = 100.0;
    double powerPct = 100.0;
    double waterPct = 100.0;
};
