# M3c — field panel content and map route/hazard/target overlays

Turn the placeholder side panel into the astronaut's one-recommendation field
panel, and teach `MapView` to draw the hazard grid, the route, the crew start
and the science targets. This is a display milestone: do not add routing or
ranking logic here, and do not change the 20° no-go threshold.

Depends on M3a/M3b:
- `analysis/TargetRanking.h` — `ScienceTarget`, `RankedTarget`
- `routing/TraversePlanner.h` — `Route` (`points`, `distanceM`, `timeMin`,
  `maxSlopeDeg`, `standoffM`, `valid`, `warning`)

## Allowed files
- `src/ui/SidePanel.h`, `src/ui/SidePanel.cpp`
- `src/ui/MapView.h`, `src/ui/MapView.cpp`
- `tests/test_sidepanel.cpp` (new)
- `tests/test_mapview.cpp` (add slots only; keep existing tests passing)
- `tests/CMakeLists.txt` (add the sidepanel target)

Do not touch MainWindow, analysis, routing, data, assets, docs, or the root
CMakeLists.txt. Do not hard-code the target names, coordinates or why-text in
the widgets; render the passed model.

## 1. `SidePanel` content

Public API (exact):

```cpp
#include "analysis/TargetRanking.h"
#include "routing/TraversePlanner.h"

class SidePanel : public QWidget
{
    Q_OBJECT
public:
    explicit SidePanel(QWidget *parent = nullptr);

    void setScienceGoal(const QString &goal);
    void setRankedTargets(const QVector<RankedTarget> &targets);
    void setPrimaryTarget(const RankedTarget &primary, const Route &route);
    void setConsumablesText(const QString &text);
    void setWarnings(const QStringList &warnings);

    QString scienceGoalText() const;
    QString primaryTargetId() const;
    QString primaryTargetWhy() const;
    QString routeSummaryText() const;
    QStringList warningTexts() const;
    int rankedTargetCount() const;
};
```

Visuals / layout, top to bottom:

1. **FIELD PANEL** title.
2. **SCIENCE GOAL** — wraps; object name `scienceGoalLabel`.
3. **NEXT TARGET** — object `primaryTargetLabel` shows
   `"T2 · Marias Pass"`, and `primaryWhyLabel` shows the `why` text.
   If no primary is set, show `"NO TARGET"` / `"No reachable target."`.
4. **RANKED TARGETS** — one compact row per `RankedTarget`, in the passed
   order. Object names `targetRow_<id>`. Each row text must include the id,
   name, travel minutes, minutes on target and value rate
   (e.g. `"T2 · Marias Pass — 5 min travel · 14 min on target · 0.050/min"`).
   If `reachable == false`, say `"UNREACHABLE"` instead of a rate.
   The primary row should be visually distinct (amber border/background).
   Keep the rows as `QLabel`s in a vertical layout; they are large, glove-
   friendly rows, not a tiny table.
5. **ROUTE** — object `routeSummaryLabel`.
   - valid: `"ROUTE 412 m · ~21 min · max slope 9° · standoff 0 m"` using
     `Route` fields. If `standoffM > 0`, still show it.
   - invalid: `"NO SAFE ROUTE"` plus `Route::warning` if present.
   - **Never claim walkback or consumables are satisfied.** Add a small line
     `"Walkback: pending M4"` (object `walkbackNote`), not a green check.
6. **CONSUMABLES** — object `consumablesLabel`; render `setConsumablesText()`
   verbatim, default `"Not linked yet (M4)"`.
7. **WARNINGS** — object `warningLabel`, one line per warning with a
   `"! "` prefix and a red/dark-red background. Default text:
   `"! No-go: slope > 20° or unknown data"` so the rule is always visible.
8. Stretch at the bottom.

Use the field-equipment visual language already used in the app: dark panel,
light text, amber section headers, large fonts, generous spacing. Avoid a
mission-control dashboard look. Minimum width 300 px.

## 2. `MapView` overlays

Add to the public API:

```cpp
#include "analysis/TargetRanking.h"
#include "routing/TraversePlanner.h"

void setRoute(const Route &route);
void clearRoute();
void setTargets(const QVector<RankedTarget> &targets, const QString &primaryId);
void setCrewStart(const QPointF &eastNorth);
void setHazardOverlay(const QImage &image, const QRectF &extentMeters);
void clearHazardOverlay();
void fitToRoute();

const Route &route() const;
QVector<RankedTarget> targets() const;
QPointF crewStart() const;
```

Implementation:
- Store a `Route`, a `QVector<RankedTarget>`, a primary id, a crew start,
  a `QImage` hazard overlay and its extent.
- Draw **after** the raster layers, **before** the north arrow/scale/attribution:
  1. hazard overlay at opacity ~0.30, mapped from its extent metres to the
     current view. Row 0 is north; do not flip it.
  2. route: dark casing (6 px) then bright cyan core (3 px). Draw only when
     `route.valid` and `points.size() >= 2`.
  3. crew start: green filled circle with a dark outline and `"START"` label.
  4. targets: amber circle for reachable, red ring for unreachable/no-go;
     the primary target is larger and orange with its `id · name` label.
     Use a dark translucent label backing for legibility. Do not draw labels
     completely outside the viewport.
- `fitToRoute()`: if the route is valid, fit its bounding box with 15% margin;
  enforce a minimum span of 500 m per axis (so a 138 m route still shows
  context, not a 0.25 m/px close-up); clamp through the existing mpp limits.
  If no valid route, do nothing.
- `setRoute` / `clearRoute` / `setTargets` / `setHazardOverlay` must call
  `update()`. `clearHazardOverlay()` drops the image and extent.
- Do not change `visibleLayersChanged`, cursor/pan/zoom, fit-to-scene or the
  M2 draw order. `route()` returns the stored copy.

## 3. Tests

`tests/test_sidepanel.cpp` (new target, offscreen):
1. `rendersOneRecommendation()` — set a goal, three synthetic ranked targets
   (T1/T2/T3, one unreachable), primary T2, valid route
   `distanceM=412`, `timeMin=21`, `maxSlopeDeg=9`, `standoffM=0`, no warning.
   Verify `primaryTargetId()=="T2"`, `primaryTargetWhy()` contains the passed
   text, `routeSummaryText()` contains `"412"`, `"21"`, `"9"`,
   `rankedTargetCount()==3`, `targetRow_T2` exists and contains `"Marias"`,
   `targetRow_T3` contains `"UNREACHABLE"`, and the default warning contains
   `"20"`.
2. `showsNoSafeRoute()` — invalid route with a warning; verify
   `routeSummaryText()` contains `"NO SAFE ROUTE"` and the warning text is
   present; `walkbackNote` exists and does not claim success.
3. `rendersPassedWarnings()` — set two warnings; `warningTexts().size()==2`.
   Do not assert exact stylesheet colors.

`tests/test_mapview.cpp` additions:
4. `drawsRouteAndTargets()` — use the existing real-raster helper; after fit,
   build a route from two points inside `visibleExtentMeters()` (e.g. centre
   to centre + 100 m east), with a `RankedTarget` at the end; render before
   and after `setRoute`/`setTargets`/`setCrewStart`; `QVERIFY(before != after)`.
5. `drawsHazardOverlay()` — create a 8×8 opaque red `QImage` and set it over
   the central quarter of the visible extent; render before/after; images
   differ.
6. `fitToRouteShowsContext()` — a short 100 m route; `fitToRoute()` must put
   the map at a scale whose visible width is at least 400 m (context), and
   the route must still be inside the visible extent.

## Verify
```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```
All tests must pass; report the commands and any QSKIP reason. Do not claim a
visual check you did not perform.
