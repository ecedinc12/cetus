#pragma once

#include <QTime>

// Plain mission-state snapshot (AGENTS.md §6: one object, passed explicitly).
// M0: data only — no logic. Values below are the demo scenario S0 (PLAN.md §6).
struct MissionState
{
    int sol = 3;                 // Sol number of the current EVA.
    QTime clock = QTime(8, 40);  // Local Mars solar time on the surface.
    double tau = 0.6;            // Dust optical depth (dimensionless).

    // Consumables, percent remaining (suit / rover margins).
    double o2Pct = 100.0;
    double powerPct = 100.0;
    double waterPct = 100.0;
};
