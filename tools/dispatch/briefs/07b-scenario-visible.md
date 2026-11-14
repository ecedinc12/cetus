# Brief 07b — make scenario changes visible on the map, with an explicit "what changed" line

The merged M4/M5 scenario currently only changes the panel/status text. The
multi-target plan exists (`MissionPlan`) but only the **first leg** is drawn;
when S2 cuts the budget from 360 to 90 min the plan really does drop T2, yet
the map looks identical. S1 and S3 are intentionally plan-neutral, which is
even more confusing without an explicit explanation.

Fix this by drawing the whole planned traverse and adding a compact scenario
context line that says what changed. Also shorten the repeat-heavy walkback
warnings so the ROUTE section stays above the fold at 1280×800.

Do not change any ranking, walkback, route, hazard, or scenario number. Do not
claim a new measured data kind.

## Files you may change

- `src/app/ScenarioSteps.h`, `src/app/ScenarioSteps.cpp` (add a display title only)
- `src/ui/MapView.h`, `src/ui/MapView.cpp`
- `src/ui/MainWindow.h`, `src/ui/MainWindow.cpp`
- `src/ui/SidePanel.h`, `src/ui/SidePanel.cpp`
- `tests/test_scenario.cpp`
- `tests/test_ui.cpp`
- `tests/test_mapview.cpp`

Do **not** touch `LayerLegend` or `HazardMap` (another worker owns the map
key), `docs/`, `assets/`, `data/`, `tools/`, `logs/`, root `*.md`, or any
sample-logging file. Keep the M6 COLLECT action exactly as it is.

## 1. Add a scenario display title

In `ScenarioStep` add:

```cpp
QString title;   // short display label, e.g. "DUST STORM"
```

Fill in `ScenarioSteps.cpp` without changing tau, budget, flags or banners:

- S0 `NOMINAL`
- S1 `DUST RISING`
- S2 `DUST STORM`
- S3 `COMMS CLOSED`
- S4 `POWER LOW`

## 2. Scenario context line in `SidePanel`

Add, directly under the `FIELD PANEL` title and before `SCIENCE GOAL`:

```cpp
void setScenarioContext(const QString &stepId, const QString &title,
                        const QString &detail, int stepIndex, int stepCount);
QString scenarioContextText() const;
```

New label `QLabel *m_scenarioLabel`, object name `scenarioContextLabel`,
word-wrapped, visible in every step. Style it as a field-equipment banner
(dark background, amber 1–2 px border, light text, bold first line); it must
be readable but compact. Suggested text:

```text
SCENARIO S2 OF S4 · DUST STORM
Storm: EVA budget 360 → 90 min · plan now T1.
```

Use `stepIndex + 1` and `stepCount` for the `OF` part. Include the label in
`updateWrappedHeights()` so it is not clipped. Do not move the existing
safety-first order: title, scenario context, SCIENCE GOAL, WARNINGS, ROUTE,
NEXT TARGET, COLLECT, CONSUMABLES, RANKED TARGETS.

## 3. Draw the whole multi-target plan

In `MapView` add:

```cpp
void setPlanRoutes(const QVector<Route> &routes);
void clearPlanRoutes();
const QVector<Route> &planRoutes() const;
```

Semantics:

- `routes` are the plan legs in order, starting with the primary leg
  (start → first target), then target-to-target legs.
- Draw the primary leg through the existing `m_route` path exactly as now.
- Draw `routes.at(1)` onward as a **dashed lighter-cyan** polyline with a
  dark casing (5 px casing / 3 px core), underneath the primary route so the
  next leg reads as the immediate action.
- Only draw when `route.valid` and at least two points.
- `fitToRoute()` must include all points of `m_planRoutes`, not just
  `m_route`, so S0/S1 frame the full T1 → T2 plan and S2 reframes to T1.
- Clear plan routes on S4 via `clearPlanRoutes()`.

Also make every science target legible, not just the primary:

- keep the existing primary marker/label;
- label non-primary targets with at least their id (`T2`, `T3`);
- draw `walkbackFailed` targets with a red ring/accent and a short label
  suffix such as `T2 · WALKBACK FAIL`;
- draw unreachable targets with the existing red ring and `T3 · NO ROUTE`;
- keep the labels inside the viewport rule already used by `drawLabel`.

Do not change `setRoute`, `setTargets`, `route()`, `targets()`, or existing
marker behaviour used by tests.

## 4. Wire the plan into `MainWindow`

In `MainWindow.h` add a route cache alongside `m_legByPair`:

```cpp
QHash<QString, Route> m_legRouteByPair;
```

In `loadMission()`’s target-pair loop, store the actual `Route` in addition to
the minutes. In `replan()` after `m_plan` is built:

- if `m_plan.steps` is empty -> `clearPlanRoutes()`;
- otherwise build `QVector<Route>`:
  - first leg = `m_routeById.value(m_plan.steps.first().targetId)`;
  - each subsequent leg = `m_legRouteByPair.value(pairKey(prevId, nextId))`;
  - append only valid routes;
- call `m_mapView->setPlanRoutes(...)`.

Keep `m_mapView->setRoute(m_route)` unchanged. In S4 both the route and plan
routes are absent.

Build the context detail from the **actual recomputed plan**, not a copied
sentence. Required content per step:

- S0: `Full plan: <summary> · <budget> min EVA budget.`
- S1: `Dust rising. Plan unchanged: <summary> · <budget> min budget.`
- S2: `Storm: EVA budget <old> → <new> min · plan now <summary>.`
  Use `Walkback::kTimeBudgetMin` and the current `m_state.timeBudgetMin`.
- S3: `Comms offline +<minutes> min · plan unchanged: <summary>.`
- S4: `Power low: science disabled · return to the EVA start (haven).`

Call `m_sidePanel->setScenarioContext(step.id, step.title, detail,
m_stepIndex, ScenarioSteps::count())` on every replan, before warnings.

## 5. Keep warnings compact

The S2 warning block currently repeats the whole scenario banner and the full
walkback sentence, pushing ROUTE below the fold. Make these changes:

- **Do not** append `step.banner` to `warnings`; it now lives in the scenario
  context line. Keep `ScenarioStep::banner` in the table and its existing test.
- For a walkback failure append one short line, derived from the real
  `WalkbackResult`:
  `"T2 Marias Pass: walkback fails by 18 min (90 min budget)."`
  (use `qRound(-result.marginMin)` and `qRound(m_state.timeBudgetMin)`).
- Keep `"<id> <name> has no safe route."` for unreachable targets.
- Shorten the unknown line to
  `"26% of hazard grid has no data — treated as no-go."` (still computed from
  `stats()`, never hard-coded).
- Keep every real warning; only remove duplicated prose.

## 6. Tests

`tests/test_scenario.cpp`:

- assert every step has a non-empty `title` and the exact titles above;
- keep all existing tau/budget/flag/banner assertions untouched.

`tests/test_ui.cpp`:

- extend `planChainsTargetsWithinBudget()`: the map’s `planRoutes()` has 2
  legs at S0, 1 at S2, and is empty at S4;
- add `scenarioChangeIsVisibleAndExplained()`:
  - S0 `scenarioContextText()` contains `S0`, `NOMINAL`, `Full plan`, `T1`,
    `T2`;
  - S1 context contains `DUST RISING` and `Plan unchanged`; plan routes stay 2;
  - S2 context contains `DUST STORM`, `360`, `90`, `plan now T1`; plan routes
    become 1 and `w.route()` still ends at T1;
  - S3 context contains `COMMS CLOSED`, `offline`, `plan unchanged`; plan
    routes stay 1;
  - S4 context contains `POWER LOW`, `science disabled`; plan routes empty and
    `!w.route().valid`;
  - each `scenarioContextLabel` is found by object name.
- add `routeSectionStaysAboveTheFold()`:
  - build 1280×800, show, advance to S2, wait for layout;
  - find `routeSummaryLabel` and `sidePanelScroll`; map the route label’s
    top-left into the scroll viewport and assert its `y()` is less than the
    viewport height (the route header is not below the fold);
  - also assert the compact T2 warning contains `walkback fails by` and does
    **not** equal the long scenario banner.

`tests/test_mapview.cpp`:

- add `fitToRouteIncludesPlanLegs()` with synthetic routes: primary route near
  the origin, a second leg far away; call `setPlanRoutes({primary, second})`
  and `fitToRoute()`; assert the visible extent contains the far leg’s end;
- add `planRoutesCanBeCleared()`: set, verify size, `clearPlanRoutes()`, size 0;
- keep existing route/marker tests unchanged.

## Verification

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Report exact totals. No screenshot or visual-check claim.
