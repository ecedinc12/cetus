# Brief 07c — keep the route and walkback visible at 1280×800

After 07b the scenario banner is honest and the whole plan is drawn, but the
field panel still spends too much vertical space above `ROUTE`:

- scenario banner ~84 px,
- science goal ~100 px,
- S2 warnings ~158 px,
- `ROUTE` starts around y=421 in a 482 px viewport, so the walkback line is
  clipped.

Tighten the field panel only. Do not change any text content, thresholds,
ranking, routes, status wording, map drawing or sample logging.

## Files you may change

- `src/ui/SidePanel.cpp` (styles, width, scenario heading only)
- `tests/test_ui.cpp` (the above-fold assertion only)

Do not touch `src/ui/SidePanel.h` unless a compile error forces it, and do not
touch `MainWindow`, `MapView`, `LayerLegend`, `HazardMap`, scenario data or any
sample-logging file. No docs, assets, data, tools, logs or root Markdown.

## Changes

1. `SidePanel` minimum width: `300` -> `340`. The dock will follow it. The map
   still has plenty of width at 1280.

2. Scenario label: make the heading one line at 340 px. In
   `setScenarioContext()` replace the heading format with:

   ```cpp
   QStringLiteral("SCENARIO %1 \u00b7 %2 \u00b7 %3/%4")
       .arg(stepId, title).arg(stepIndex + 1).arg(stepCount)
   ```

   so it reads `SCENARIO S2 · DUST STORM · 3/5` (still contains the step id and
   title used by tests). Reduce the label font from `13px` to `12px` and the
   padding from `4px` to `3px`. Keep the amber border and dark backing.

3. Reduce the largest text blocks by 1 px without changing wording:
   - science goal `15px` -> `14px`;
   - warnings `13px` -> `12px`, padding `6px` -> `4px`;
   - primary why `14px` -> `13px`;
   - consumables `14px` -> `13px`.
   Keep route summary at `15px`, primary target at `18px`, section headers
   unchanged, and the COLLECT button at 44 px (glove target).

4. Do not remove or reorder any section. FIELD PANEL -> scenario banner ->
   SCIENCE GOAL -> WARNINGS -> ROUTE (+ walkback) -> NEXT TARGET -> COLLECT ->
   CONSUMABLES -> RANKED TARGETS must stay.

## Test

In `tests/test_ui.cpp::routeSectionStaysAboveTheFold()` (S2, 1280×800, after
layout):

- keep the existing route-header assertion;
- find `walkbackNote`; map its top-left into `sidePanelScroll`'s viewport and
  assert the **whole** label is visible:
  `walkTop.y() >= 0 && walkTop.y() + walkback->height() <= viewportHeight`;
- keep the compact-warning assertion.

If the whole walkback still does not fit, tighten the styles above further
(font/padding/margins only) until it does; do not hide text or move WARNINGS
below ROUTE.

## Verification

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Report the exact result. No screenshot claim.
