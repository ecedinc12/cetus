# Cetus: M4/M5 progress log (for the next Claude session)

READ THIS FIRST, then AGENTS.md and PLAN.md. Update this file after every step.
Rule from the user: do not claim a build/test/visual check that was not actually run.
Claude here can read/write files via the Filesystem MCP but CANNOT build or run
the app. The user runs commands on their machine and pastes the output back.

## How we work
- User (Esnaf) writes Turkish, is a terminal/git beginner. Give short, copy-paste,
  one-task-at-a-time steps, one sentence on what each command does. No em-dashes.
- Codex / DeepSeek workers are NOT used. Claude writes the code directly into
  ~/claudeaccess/cetus (src/, tests/), the user builds and runs ctest.
- Repo: https://github.com/ecedinc12/cetus.git (cloned at ~/claudeaccess/cetus).
  It is a friend's repo; the user does M4 then M5 back to back.
- Build: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release ; cmake --build build -j ;
  ctest --test-dir build --output-on-failure ; ./build/cetus
- Env: Arch (Omarchy, Wayland), Qt 6.12.0, GDAL 3.13.3.

## State when M4 started (read from files, not built by Claude)
- M0 to M3 exist in the tree: rasters, layers, HazardMap, TargetRanking,
  TraversePlanner (A*), MissionScenario (assets/scenario/m3_scenario.json),
  SidePanel, MainWindow wiring. Whether it builds/passes on the user's machine is
  NOT yet confirmed in this log.
- MissionState (src/app/MissionState.h) is data only: sol, clock, tau, o2/power/water pct.
- TraversePlanner.h says walkback/consumables are M4 and must not be claimed before.
- Scenario: crew START at (-753.40, -276224.80) m; T1 Pahrump Hills, T2 Marias Pass,
  T3 Vera Rubin Ridge (value 0.90/0.95/0.85, minutes_on_target 12/14/18).
- Rough straight-line distances from START: T1 ~138 m, T2 ~580 m, T3 ~3.6 km.
  At 1.2 km/h: T1 ~7 min, T2 ~17 min, T3 ~3 h one way. These are estimates, not measured routes.

## Decisions (user confirmed)
- Safe haven for walkback = the EVA start point (START). There is no real lander or
  shelter coordinate in the data; do not invent one. Label it a design choice.
- time_budget_min = 360 [tunable design choice, not a NASA value]. Chosen so T3 is
  rejected by the walkback rule in the demo.
- Walkback rule (PLAN.md 5.4): time to haven = 1.2 x path time, plus 15 min reserve
  [tunable]. A plan violating it is invalid: drop the farthest target and replan.
- Missing data = unknown, never safe. Anything not from NASA is labeled design choice/[tunable].

## Known inconsistencies to handle (do not hide)
- PLAN.md section 6 says S0 recommends T2. With the real targets, value-rate arithmetic
  gives T1 first (rate ~0.048 vs T2 ~0.030) and tests/test_ui.cpp asserts T1. PLAN.md is
  stale here; keep T1 for S0 and update the S0..S4 expectations when M5 is written.
- tau has no defined effect on speed/power/visibility yet. M5 needs an explicit,
  documented [tunable] rule (ask the user before inventing numbers).
- Consumables O2/power/water: no endurance data. M4 should use the time budget, not
  invented per-resource burn rates, unless the user supplies numbers.

## Plan
M4 (PLAN.md): consumables + walkback + suit speed; panel updates when route/target changes.
 - M4-1: planner/analysis: walkback check function + constants (src/routing or src/app), unit tests.
 - M4-2: MissionState gets time budget/elapsed; consumables computed from the plan.
 - M4-3: MainWindow/SidePanel show walkback status next to the route; replan drops farthest target on violation.
M5 (PLAN.md): scenario state machine (tau rise) re-ranks and reroutes with one control.
 - M5-1: scenario steps S0..S4 in one header, deterministic; tests S0->S4.
 - M5-2: "Advance scenario" control, banner explaining the replan.

## Step log (append newest at the bottom)
- [not built] Created this file. No M4 code written yet.
- [run by user, output pasted] Clean clone, build OK, ctest 6/8 pass, analysis and routing FAIL. Cause: assets/layers/ does not exist in the clone (no .gitignore, no data/ dir either), so GDAL cannot open gale_hirise_slope_4m.tif. This is missing data, not a code bug. Caution: scaffold/raster/mapview/ui/layerlegend/sidepanel "Passed" may be skipping data-dependent checks when rasters are absent; they are NOT proof the map works until rasters exist.
- Needed before M4: the 4 prepared rasters (gale_hirise_ortho_1m.tif, gale_hirise_dem_4m.tif, gale_hirise_slope_4m.tif, gale_hirise_hillshade_4m.tif) in assets/layers/. Either get the folder from the friend (ecedinc12) or re-run the prep commands in docs/data-sources.md section 3 (downloads HiRISE ortho + DTM from hirise-pds.lpl.arizona.edu).
- Reference numbers from docs/data-sources.md 2.5 (author's own measurements, not re-run by us): T1 138.3 m / 7.5 min, T2 36.0 min travel, T3 211.6 min travel. With the walkback rule T3 needs about 211.6 + 18 + 1.2*211.6 + 15 = ~499 min, so it is rejected at both 360 and 480 min budgets; T2 needs about 108 min.
- [run by user] After rasters were produced from docs/data-sources.md section 3 (data/raw has the HiRISE JP2/IMG/LBL; assets/layers has the 4 tifs), ctest = 8/8 passed. Test times rose (ui 0.18->0.93 s, mapview 0.13->0.55 s), consistent with them previously skipping data work. Visual check of the running app still NOT done by Claude.
- [written by Claude, NOT built or run yet] M4-1: src/routing/Walkback.{h,cpp} (pure arithmetic: elapsed + leg + work + 1.2*return + 15 min reserve <= 360 min budget; constants are [tunable]), tests/test_walkback.cpp (hand-computed cases + a real-data test expecting T1,T2 feasible and T3 rejected, which relies on docs numbers and must be confirmed by running), CMake entries in CMakeLists.txt and tests/CMakeLists.txt. Return path time is taken equal to the outbound path time (edge cost is symmetric).
- Next: M4-2 (MissionState time/consumables from the plan), M4-3 (MainWindow/SidePanel walkback status, drop-farthest-and-replan).
- [run by user] M4-1 built: test_walkback 9/9 passed. Real-data lines printed by the test: T1 travel 7.5 min margin 316.5 feasible; T2 travel 36.0 min margin 251.8 feasible; T3 travel 211.6 min margin -138.6 NOT feasible (matches the docs numbers).
- [written by Claude, NOT built or run yet] M4-2/M4-3: RankedTarget.walkbackFailed (TargetRanking.h); SidePanel::setWalkbackText/walkbackText + "WALKBACK FAIL" row text (the default walkback label is now "Walkback: pending (no plan to check)", keeps the old test contract: contains "pending", never "safe"); MainWindow::loadMission runs Walkback::evaluate per reachable target, skips failed ones when choosing the primary, writes a walkback line, a time-based consumables line (O2/power/water say "not modeled (no endurance data)", no invented burn rates) and a warning per failed target. New tests: test_ui walkbackRejectsFarTarget, test_sidepanel showsWalkbackStatus. Static consumables snapshot (O2/POWER/WATER 100%) was removed from the panel because it implied data we do not have.
- Not done yet for M4: MissionState has no elapsed/time-budget fields (MainWindow owns no MissionState; plan to add it in M5 together with the scenario steps). Greedy multi-target plan (PLAN 5.5 steps 3-4) is not implemented; only "primary = best-ranked target that passes walkback". Visual check in the running app not done.
- [run by user] M4 done: ctest 9/9 passed after M4-2/M4-3; user opened ./build/cetus and confirmed visually that everything looked right (walkback line, T3 flagged, consumables text). Remaining M4 gaps: no MissionState elapsed/budget fields, no greedy multi-target plan.
- M5 design question (asked to user): with the real values T1 is the best target at every step (rate ~0.048 vs T2 ~0.019 at 36 min travel), so a tau step cannot flip the primary without changing design values. Option A (recommended): tau rise shrinks the EVA time budget / slows speed so T2 fails walkback while T1 stays primary; banner explains. Option B: edit science_value/minutes_on_target in assets/scenario/m3_scenario.json so S0 starts on T2 (contrived, labeled [tunable]). PLAN.md S0 says "recommends T2" which is stale.
- [written by Claude] M5 (decision: Option A, user said "en iyi sekilde yap"): src/app/ScenarioSteps.{h,cpp} (S0..S4 table), MissionState gets timeBudgetMin/commsClosed/commsOfflineMin, MainWindow gets replan()/advanceScenario() and an "Advance scenario" toolbar button, tests/test_scenario.cpp. Tau rise does not change the primary target: S2 (tau 1.1) cuts the EVA budget 360 -> 90 min [tunable design choice], so T2 (~108 min needed incl. walkback) fails walkback and T1 (~44 min) stays primary. S3 shows comms offline +40 min (no functional change). S4 is a scripted power-low event (not modeled): science disabled, return to haven (EVA start).
- [run by user] M5 built: ctest 10/10 passed. Visual check of the Advance scenario button (S0..S4 and wrap back to S0) NOT done yet. Known gaps: no test drives MainWindow::advanceScenario (test_ui not checked for it); PLAN.md section 6 still says S0 recommends T2 (stale, S0 recommends T1).
- [seen by user, screenshots pasted] Visual check: S0 and S4 look right. S0: route 138 m ~7 min, walkback margin 317 min, T3 WALKBACK FAIL (139 min over 360), ranking T1 0.046/min, T2 0.019/min (the earlier "~0.048" in these notes was a rough estimate; 0.90/(7.5+12) = 0.046). S4: science disabled, NEXT TARGET none, T2 18 min over 90, T3 409 min over 90. BUG found in S4: ROUTE header says "NO SAFE ROUTE" although a safe route exists (science is disabled on purpose); the line below it is correct. S1, S2, S3 not yet looked at. Minor: consumables "19 of 360 min" is 19.5 rounded to 0 decimals.
- [written by Claude, NOT built or run yet] Fix for the S4 ROUTE header bug: SidePanel::setRouteSummaryText() added; MainWindow::replan overrides the header after setPrimaryTarget: S4 -> "SCIENCE DISABLED: no science route (scripted power event)"; no target passes walkback while routes exist -> "NO TARGET WITHIN BUDGET ...". New tests: test_sidepanel routeSummaryCanBeOverridden, test_ui advanceScenarioWalksS0ToS4AndWraps (drives advanceScenario S0..S4 and the wrap, checks primary, walkback flags, budget 90/tau 1.1 at S2, comms at S3, ROUTE header at S4). This also closes the "no test drives advanceScenario" gap once it passes.
- Still open: visual check of S1, S2, S3; PLAN.md section 6 S0..S4 text (still says S0 recommends T2); consumables "19 of 360" shows 19.5 rounded to 0 decimals (cosmetic).
- [run by user] ROUTE header fix + new tests built: ctest 10/10 passed (ui 1.06 s, sidepanel 0.18 s, scenario 0.06 s), including advanceScenarioWalksS0ToS4AndWraps and routeSummaryCanBeOverridden. The S4 header now reads SCIENCE DISABLED in tests; not yet confirmed by eye in the running app. S1, S2, S3 visual check still open.
- [written by Claude] PLAN.md section 6 table and an "as built" note updated to match the implemented S0..S4 (S0 recommends T1, S2 = budget cut to 90 min, S4 header). Docs only, no code change.
- [written by Claude, NOT built or run yet] M4 gaps closed (user: "m4 ve m5 yapcaktik"): (1) MissionState::elapsedMin (0 in the demo, honoured by walkback and the plan); (2) greedy multi-target plan, PLAN.md 5.5 steps 3-4: src/routing/MissionPlan.{h,cpp} (pure arithmetic, leg times injected), MainWindow plans the 3 target-to-target legs once in loadMission (m_legByPair, symmetric cost) and builds the plan per scenario step in replan (empty in S4); the consumables line now reads "PLAN: T1 -> T2. TIME: .. of .. min EVA budget used after the plan, walkback margin .. min ..". tests/test_missionplan.cpp (hand-computed cases) + CMake entries, test_ui planChainsTargetsWithinBudget (expects S0 plan T1 -> T2, T3 excluded; S2 plan T1 only; S4 empty). Limits: the map still draws only the first leg (start -> primary target), the panel lists the whole chain; no 2-opt pass (5.5 step 5); the expected S0 chain T1 -> T2 comes from reasoning about the docs numbers and must be confirmed by running.
- [run by user] Greedy plan + elapsedMin built: ctest 11/11 passed (new: missionplan, ui planChainsTargetsWithinBudget). So S0 plan = T1 -> T2, S2 plan = T1 only, S4 plan empty are confirmed by the test run. Not yet confirmed by eye in the running app: the "PLAN: T1 -> T2" consumables line, and S1/S2/S3/S4 screens (including the S4 ROUTE header). Remaining for M4/M5 sign-off: that visual pass. Known limits: map draws only the first leg; no 2-opt pass.
- [written by Claude, NOT built or run yet] Map overlay toggles re-applied on branch ui-scenario-map-key-fixes (the earlier version lived on m4-m5-walkback-scenario and could not be cherry-picked because MapView/MainWindow changed): MapView::setHazardVisible/isHazardVisible, setTargetsVisible/areTargetsVisible (default on; route legs and crew start stay drawn), checkable toolbar buttons "Hazard" and "Targets" (objectNames hazardButton, targetsButton) with colour tooltips, test_ui mapOverlaysCanBeHidden.
