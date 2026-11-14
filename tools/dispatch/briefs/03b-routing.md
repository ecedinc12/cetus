# M3b — hazard-weighted traverse planner

Implement the route solver. It must be a science-supporting traverse planner,
not a plain shortest path: slope/unknown cells raise cost, hard no-go cells are
never entered, and a target on a no-go slope is reached from a documented safe
standoff.

This brief assumes `src/analysis/HazardMap.h` from M3a exists with:

```cpp
QRectF extentMeters() const;   // x=west, y=south, bottom=north
double cellSizeM() const; int width() const; int height() const;
bool contains(double east,double north) const;
double slopeDeg(int col,int row) const;       // NaN unknown
bool isNoGo(int col,int row) const;           // unknown/outside true
double cost(int col,int row) const;           // 1..3 traversable, inf no-go
QPointF cellCenter(int col,int row) const;
```

and `src/analysis/TargetRanking.h` with `struct ScienceTarget` (`id`, `name`,
`eastingM`, `northingM`, `position()`).

## Allowed files
- `src/routing/TraversePlanner.h`, `src/routing/TraversePlanner.cpp`
- `tests/test_routing.cpp` (new)
- `tests/CMakeLists.txt` (add the test target)

Do not touch analysis, UI, data, assets, docs, or the root CMakeLists.txt.
Do not change the hazard threshold (20°) or invent a second one.

## Header

Replace the placeholder with:

```cpp
#pragma once

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
```

Do **not** add walkback or consumables here; that is M4. The route must not
claim walkback is satisfied.

## Solver rules

- Grid graph: one node per traversable hazard cell, 8-connected.
- Edge cost = `0.5 * (cost(a) + cost(b)) * geometricDistance(a,b)`.
  A diagonal edge may **not** cut a corner: if either orthogonal neighbour is
  no-go/unknown, reject that diagonal. This prevents the line squeezing through
  a one-cell gap that the crew cannot walk.
- A* with an admissible heuristic: Euclidean distance in metres times the
  minimum cost multiplier (1.0). Use `std::priority_queue`, deterministic
  tie-breaking (lower f, then lower cell index) so tests are stable.
- `timeMin = weightedCostDistance / kSuitSpeedMps / 60.0`.
- `distanceM` is plain geometric length along `points`, not weighted.
- `maxSlopeDeg` is the maximum finite `slopeDeg()` on the route cells. Since
  no-go cells are excluded, it must be <= 20° except for floating-point noise.
- Sample `slopeDeg()` only at traversable cells; never turn NaN into 0°.
- The hazard grid's actual cell size is `extentMeters().width()/width()` and
  `extentMeters().height()/height()` (the nominal 10 m is rounded up to whole
  cells). Use those actual widths for step distances; do not assume exactly
  10 m. `cellCenter()` is authoritative for point coordinates.

## Start / goal snapping

- If the requested point is outside the grid or on a no-go cell, search the
  nearest traversable cell within `kMaxSnapM` (Chebyshev ring search; choose
  the smallest Euclidean distance from the requested point to the cell centre).
- `standoffM` is the distance from the **requested goal/end** to the snapped
  route end. For `plan()` this is the requested goal; for `planToTarget()` it
  is `target.position()`.
- If no safe cell is found, return an invalid route with a specific warning.
- Start and goal may snap to the same cell: return a one-point valid route with
  zero distance/time and the correct standoff.
- `planToTarget()` should add a warning when `standoffM > 0`, e.g.
  `"Marias Pass pin is on a no-go slope; route stops 34 m short at a safe standoff."`
  Use the target name from the struct, not a hard-coded name.
- Do not silently move the target. The standoff is visible in `Route` and the
  UI will display it.

## Tests (`tests/test_routing.cpp`)

Use the real slope raster and `assets/scenario/m3_scenario.json`. Build a
`HazardMap` at 10 m (or use `HazardMap::kCellSizeM`). Required slots:

1. `plansSafeRouteToPahrump()` — start `(-753.397,-276224.802)`, goal
   `(-756.135,-276363.057)`; valid; distance 110..180 m; time 4..15 min;
   maxSlope <= 20.0 + 1e-6; every point is inside the grid and not no-go;
   `standoffM < 5`; warning empty.
2. `mariasPassIsReachable()` — target T2 from the scenario; valid;
   `standoffM < 5`; warning empty; maxSlope <= 20.0 + 1e-6. The corrected
   4 m slope at the Marias pin is 13.52°, so it is traversable; do not expect
   a standoff here.
3. `noGoTargetStopsAtStandoff()` — use a test `ScienceTarget` at the real
   Mars Trek "Top of Vera Rubin Ridge" pin: easting `-1023.67402622707`,
   northing `-279995.295943866`, name `"Top of Vera Rubin Ridge"`. Its real
   4 m slope is 33.92°, so route `valid` must be true, route end must not be
   no-go, `standoffM > 0`, and warning must contain the target name and
   `"standoff"`. This tests the hard no-go rule on real terrain.
4. `plansLongerRouteToVeraRubin()` — target T3; valid; `distanceM` is greater
   than the Pahrump route (it is ~3.6 km straight-line); time is positive;
   warning empty or standoff-only.
5. `rejectsDisconnectedStart()` — a point far outside the grid (e.g. 50 km
   east) returns `valid == false` and a non-empty warning; do not crash.
6. `routeIsDeterministic()` — planning the same Pahrump route twice gives
   identical point count and endpoints.

Add the test target to `tests/CMakeLists.txt` with `QT_QPA_PLATFORM=offscreen`
and `CETUS_ASSET_DIR=${CMAKE_SOURCE_DIR}/assets`.

## Verify
```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```
All tests (scaffold, raster, mapview, layerlegend, ui, analysis, routing) must
pass. Report commands actually run; never claim an unrun test.
