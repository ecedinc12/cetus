# M3d — wire the mission into MainWindow

Load the NASA-derived M3 scenario, build the hazard grid, plan one route per
target, rank by value rate, and present exactly one recommendation in the
field panel and on the map. The app must still open and stay usable when
mission data fails, with a loud status message (same policy as raster load).

Depends on M3a/M3b/M3c APIs:
- `MissionScenario::loadFromFile`, `SiteConfig::scenarioPath()`
- `HazardMap::build`, `HazardMap::hazardImage`, `HazardMap::stats`
- `TargetRanking::rank`, `RankedTarget`
- `TraversePlanner::planToTarget`, `Route`
- `SidePanel::setScienceGoal/setRankedTargets/setPrimaryTarget/setConsumablesText/setWarnings`
- `MapView::setCrewStart/setTargets/setRoute/setHazardOverlay/fitToRoute`

## Allowed files
- `src/ui/MainWindow.h`, `src/ui/MainWindow.cpp`
- `tests/test_ui.cpp`

Do not touch SidePanel, MapView, analysis, routing, data, assets, docs, or
CMake. Do not change the 20° no-go threshold or the scenario JSON.

## MainWindow members and accessors

Add:

```cpp
bool scenarioLoaded() const;                 // true only when scenario + hazard + a valid primary route
const MissionScenario &scenario() const;
const HazardMap &hazardMap() const;
const QVector<RankedTarget> &rankedTargets() const;
const Route &route() const;
QString primaryTargetId() const;
SidePanel *sidePanel() const;
```

Members: `MissionScenario m_scenario`, `HazardMap m_hazard`,
`QVector<RankedTarget> m_rankedTargets`, `Route m_route`,
`QString m_primaryTargetId`, `QString m_missionError`,
`bool m_scenarioLoaded = false`.

## Load sequence

After the four rasters load successfully, call a private
`void loadMission()` (or equivalent). If rasters failed, do not attempt it;
leave `scenarioLoaded()` false and keep the existing `RASTER LOAD FAILED`
message authoritative.

`loadMission()` steps:

1. `MissionScenario::loadFromFile(SiteConfig::scenarioPath(), &m_scenario, &error)`.
   On failure set `m_missionError`, `qCritical()`, show the error in the
   status bar, and return without crashing.
2. Find registry layer `"hirise_slope"` and `m_hazard.build(*slope,
   HazardMap::kCellSizeM, &error)`. Fail loudly the same way.
3. For every target, call
   `TraversePlanner::planToTarget(m_hazard, m_scenario.crewStart, target)`.
   Store the route by id. Put `route.valid ? route.timeMin : -1.0` into a
   `QHash<QString,double>` for ranking (negative means unreachable; never
   fabricate a travel time).
4. `m_rankedTargets = TargetRanking::rank(m_scenario.targets, travelById)`.
5. Pick the first `reachable` ranked target. If none, leave `m_route` invalid,
   set `m_primaryTargetId` empty, and still show the ranked list + warnings.
6. Set the map:
   ```cpp
   m_mapView->setCrewStart(m_scenario.crewStart);
   m_mapView->setTargets(m_rankedTargets, m_primaryTargetId);
   m_mapView->setRoute(m_route);
   const QImage hazardImage = m_hazard.hazardImage(
       QSize(m_hazard.width(), m_hazard.height()));
   m_mapView->setHazardOverlay(hazardImage, m_hazard.extentMeters());
   ```
   Keep the M2 default layer visibility (optical only); do not silently turn
   on extra rasters. The hazard is a separate translucent overlay.
7. Fill the panel:
   - Science goal (this is a **design statement**, not measured data):
     `"Gale Crater, Mount Sharp: characterize fracture-filling mineral phases and reconstruct past fluid chemistry."`
     Store it as a named `constexpr`/`const char *` with a `[design choice]`
     comment, not inline in three places.
   - `setRankedTargets(m_rankedTargets)`.
   - `setPrimaryTarget(primary, m_route)` when there is one; otherwise pass an
     empty primary and the invalid route.
   - `setConsumablesText("O2 100% · POWER 100% · WATER 100% — static snapshot (M4 links mission state)")`.
     This is the existing `MissionState` snapshot, not a claim that consumables
     are sufficient for the route.
   - `setWarnings(...)` with:
     * `m_route.warning` when non-empty;
     * one line per `RankedTarget` with `reachable == false`:
       `"<id> <name> has no safe route."` (do not invent a reason);
     * `"<n>% of the hazard grid is unknown (no data) and treated as no-go."`
       using `m_hazard.stats().unknownCount / cellCount`, rounded to whole
       percent. If zero, omit.
     The panel's default no-go line remains visible.
8. `m_scenarioLoaded = true` only if the scenario, hazard, a primary target and
   a valid primary route all exist. A scenario with zero reachable targets is
   loaded data but not a successful plan; keep it false and say so.

## Status and fit

- `updateStatus()` must keep prior messages in priority order:
  `RASTER LOAD FAILED` > `MISSION DATA FAILED` > normal summary.
  Normal summary appends the recommendation when available:
  `" · NEXT T1 Pahrump Hills 138 m"` using the primary target id/name and
  `m_route.distanceM` rounded to whole metres.
- In `showEvent`, the one-shot post-layout fit must call
  `m_mapView->fitToRoute()` when `m_route.valid`, otherwise
  `m_mapView->fitToScene()`. Do not let the M1 first-show `fitToScene()` undo
  the route view: the existing MainWindow `showEvent` timer is the final word.

## Tests (`tests/test_ui.cpp`)

Keep the existing tests passing, **except** the old
`fitsRasterOnFirstShow()`: the first post-layout fit is now `fitToRoute()` once
a valid mission route exists, so that test must become `fitsRouteOnFirstShow()`
and assert the visible extent contains every route point and has at least
400 m of context. The default visible layer list stays
`{"hirise_optical"}`. Add:

1. `plansMissionOnStartup()` — construct the window (skip if rasters missing as
   the existing tests do); `scenarioLoaded()` true; `scenario().targets.size()
   == 3`; `rankedTargets().size() == 3`; `primaryTargetId() == "T1"` (by the
   documented value-rate arithmetic); `route().valid`; route distance 110..180
   m; `sidePanel()->primaryTargetId() == "T1"`; map `targets().size() == 3`;
   map `route().valid`.
2. `panelExplainsTheRecommendation()` — the science-goal label contains
   `"Mount Sharp"` and `"fluid"`; the primary why text mentions
   `"Jarosite"` (T1's real Mars Trek text); `routeSummaryText()` contains the
   rounded route distance; warnings contain either a route warning or the
   unknown-data warning. Do not assert a warning that may legitimately be
   absent.
3. `missionFailureIsLoud()` — with a missing asset dir (the existing
   `missingAssetsFailLoudly` setup), `loadError()` is non-empty,
   `scenarioLoaded()` is false, and the status bar contains
   `RASTER LOAD FAILED`. Do not require a modal dialog.

## Verify
```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```
All tests (scaffold, raster, mapview, sidepanel, layerlegend, ui, analysis,
routing) must pass. Report the commands actually run. Never claim a test or a
visual check you did not run.
