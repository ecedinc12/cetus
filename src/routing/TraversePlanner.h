#pragma once

// Hazard-weighting traverse solver (PLAN.md §5.4–§5.5): A* over the hazard
// grid with hard no-go cells, corner-safe diagonal moves and documented
// safe-standoff snapping. This is not a plain shortest path — the traverse
// cost grows with slope and unknown cells are never entered. Walkback and
// consumables are a later milestone (M4); this module must not claim them.

#include <QPointF>
#include <QString>
#include <QVector>

class HazardMap;
struct ScienceTarget;

struct Route
{
    QVector<QPointF> points;   // projected metres, crew start -> end
    double distanceM = 0.0;     // geometric length of points
    double timeMin = 0.0;       // hazard-weighted walking time
    double maxSlopeDeg = 0.0;   // max finite slope on traversed cells
    double standoffM = 0.0;     // requested goal -> safe route end
    bool valid = false;
    QString warning;            // empty when the pin was traversable
};

class TraversePlanner
{
public:
    // [tunable] PLAN.md §5.2: suited walking speed on flat ground.
    static constexpr double kSuitSpeedMps = 1.2 * 1000.0 / 3600.0;
    // [tunable] maximum snap distance from a requested point to a safe cell.
    static constexpr double kMaxSnapM = 200.0;

    static Route plan(const HazardMap &hazard, const QPointF &start,
                      const QPointF &goal);
    static Route planToTarget(const HazardMap &hazard, const QPointF &start,
                              const ScienceTarget &target);
};
