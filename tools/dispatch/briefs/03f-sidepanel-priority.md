# Brief 03f — put safety and route above the fold

## Why

M3's panel is now scrollable and no longer clips text, but at the real dock
height (~576 px) the order is still wrong for field use: the primary target's
long "why" pushes `ROUTE`, `CONSUMABLES` and `WARNINGS` below the fold. Safety
information must be visible without scrolling (AGENTS.md §7 "hazard-first").
This is an ordering-only change; do not rewrite the panel.

## Scope

Allowed files only:

- `src/ui/SidePanel.h` (only if member declarations need reordering)
- `src/ui/SidePanel.cpp`
- `tests/test_sidepanel.cpp`
- `tests/CMakeLists.txt` (only if a new target is truly needed; it is not)

Do not change `MainWindow`, `MapView`, assets, docs, scenario data, wording of
any label, palette, fonts, object names, or public API.

## Required section order (top to bottom)

1. `FIELD PANEL` title
2. `SCIENCE GOAL` + goal text
3. `WARNINGS` + warning box
4. `ROUTE` + route summary + `walkbackNote`
5. `NEXT TARGET` + primary target + primary why
6. `CONSUMABLES`
7. `RANKED TARGETS`

Keep exactly the existing widgets, text, styles and object names; only move
the `addWidget`/`addLayout` calls and their section headers. `WARNINGS` must
come before both `ROUTE` and `NEXT TARGET` so the no-go/unknown warning is
visible at the top of the panel.

## Tests

Add `safetyAndRouteAreAboveTheFold()`:

- Build a panel with the real-ish content (science goal, three targets, a valid
  route, two warnings).
- `panel.resize(300, 600); panel.show();` + process events.
- Assert `warningLabel` and `routeSummaryLabel` are visible and their combined
  rectangles lie inside the scroll viewport at scroll position 0:
  - `warningLabel->mapTo(panel.parentWidget(), ...)` is awkward; instead use
    `panel.findChild<QScrollArea*>("sidePanelScroll")`, take
    `viewport->rect()`, and map each label to the viewport:
    `label->mapTo(scroll->viewport(), QPoint(0,0)).y() + label->height()
     <= scroll->viewport()->height() + 1`.
- Assert `warningLabel` appears above `routeSummaryLabel`:
  `warningLabel->mapTo(scroll->viewport(), QPoint(0,0)).y()
   < routeSummaryLabel->mapTo(scroll->viewport(), QPoint(0,0)).y()`.
- Assert the existing `wrappedTextIsNotClipped()` still holds.

Do not weaken existing tests.

## Verification

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Report the actual count and result. No visual check claim.
