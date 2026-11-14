# Brief 03e — field panel layout fix (M3 polish)

## Why

A real 1280x800 render of M3 shows the right-hand `SidePanel` is unusable:
the science goal is missing, the primary "why" overlaps the ranked rows, the
three ranked rows overlap each other, and the warnings box is clipped. The
panel is not scrollable, so the bottom content is unreachable.

Measured geometry from the current build at panel size 374x576:

```
scienceGoalLabel  y=81  h=38  heightForWidth=60
primaryWhyLabel  y=191 h=39  heightForWidth=95
targetRow_T1     y=268 h=34  heightForWidth=60
targetRow_T2     y=283 h=34  heightForWidth=58
targetRow_T3     y=298 h=34  heightForWidth=58
warningLabel     y=522 h=38  heightForWidth=56
```

The panel content's minimum height exceeds the dock height and the vertical
layout compresses word-wrapped `QLabel`s below their `heightForWidth`, and the
nested target-row layout below its row minimums. This is a layout bug, not a
content bug.

## Scope

Allowed files only:

- `src/ui/SidePanel.h`
- `src/ui/SidePanel.cpp`
- `tests/test_sidepanel.cpp`
- `tests/CMakeLists.txt` (only if a new test target is genuinely needed; the
  existing `sidepanel` target should be enough)

Do **not** change `MainWindow`, `MapView`, `HazardMap`, `TraversePlanner`, the
scenario, assets, docs, or any other file. Do not change the panel's visible
wording, palette, section order, object names, or public API.

## Required behaviour

1. The panel content scrolls vertically when it does not fit. Add a
   `QScrollArea` named `sidePanelScroll` as the outer child:
   - `setWidgetResizable(true)`
   - horizontal scrollbar always off
   - no frame
   - dark background matching the panel (`#1c1612`)
   - the existing content widget keeps object name `sidePanel`? **No** — keep
     `SidePanel` itself as the widget with object name `sidePanel`, put the
     content in an inner widget named `sidePanelContent`. `MainWindow` puts
     `SidePanel` in the dock unchanged.
2. No word-wrapped label may be allocated less height than it needs. A label
   whose text wraps must get at least `heightForWidth(label->width())` (plus
   existing padding), at panel widths from 300 to 480 px. Apply this to:
   science goal, primary target, primary why, route summary, consumables,
   warnings, and every ranked target row.
3. Ranked target rows must not overlap and must keep their padding/minimum
   height. Rows should grow to fit two or three wrapped lines.
4. All content must remain reachable by scrolling at panel heights down to
   400 px. The panel must still look correct at 1280x800.
5. Keep the existing public API and all object names used by tests:
   `scienceGoalLabel`, `primaryTargetLabel`, `primaryWhyLabel`,
   `routeSummaryLabel`, `walkbackNote`, `consumablesLabel`, `warningLabel`,
   `targetRow_<id>`. Add `sidePanelScroll` and `sidePanelContent` only.
6. Empty/unreachable state must not regress: `setWarnings({})` still shows the
   default no-go warning; `setPrimaryTarget` with an empty primary id must show
   `NO TARGET` rather than `" · "`.

## Implementation notes (guidance, not a straitjacket)

- `QLabel` with `setWordWrap(true)` exposes `heightForWidth()`, but
  `QVBoxLayout` will happily compress it when the widget is over-constrained.
  The scroll area removes the outer constraint; explicitly setting each
  wrapped label's `minimumHeight` to its `heightForWidth()` at the available
  content width makes the content widget's minimum height honest.
- Recompute minimum heights in `resizeEvent` after the viewport width is known
  (and again when text changes). Use a helper, e.g.
  `applyWrappedText(QLabel*, const QString&)` and `updateWrappedHeights()`,
  with a conservative fallback width (~250 px) before the first resize.
- For dynamic target rows, compute the row height from the same helper before
  adding it, then let the row's height-for-width follow on resize.
- Do not use `setFixedHeight` based on the current contents; the panel must
  survive a resize.
- Keep the panel's `setMinimumWidth(300)`.

## Tests (add to `tests/test_sidepanel.cpp`)

Add `wrappedTextIsNotClipped()`:

- Build a panel, set a realistic science goal (two lines), three ranked
  targets with long `why` text, a valid route, and two warnings.
- `panel.resize(360, 520); panel.show();` + `QCoreApplication::processEvents();`
- For every visible `QLabel` with `wordWrap()` and non-empty text:
  `QVERIFY(label->height() + 1 >= label->heightForWidth(label->width()));`
- Assert the three target rows do not overlap:
  `targetRow_T1->geometry().bottom() < targetRow_T2->geometry().top()`, etc.
- Assert `panel.findChild<QScrollArea*>("sidePanelScroll")` is non-null and
  `verticalScrollBar()->maximum() > 0` at 360x520.
- Assert `scienceGoalLabel` is visible and its height covers its wrapped text.

The existing three slots must continue to pass unchanged.

## Verification (run and report exactly)

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Report the actual test count and result. Do not claim a visual check you did
not run.
