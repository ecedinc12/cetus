# Brief 06c — make the M6 action reachable + reveal the note

M6 works and tests pass, but at the demo window size (`1280×800`) the panel's
COLLECT button sits below the fold. Add the same action to the always-visible
map toolbar and scroll the panel to the new note after a successful append.
Also fix the awkward status suffix `SAMPLE SAMPLE-3-1`.

## Files you may change

- `src/ui/MainWindow.h`, `src/ui/MainWindow.cpp`
- `src/ui/SidePanel.h`, `src/ui/SidePanel.cpp` (reveal method only)
- `tests/test_ui.cpp`

Do not touch `docs/`, `assets/`, `data/`, `tools/`, `logs/`, root `*.md`, the
sample schema/validation, or M4/M5 files.

## 1. Toolbar action

In `MainWindow` constructor, after the zoom buttons, add:

- `QToolButton *m_collectToolbarButton` with object name
  `collectSampleToolbarButton`, text `COLLECT SAMPLE`, `ToolButtonTextOnly`.
- minimum height 32; amber background (`#ffbe3c`), dark bold text, disabled
  style `#4a3d33`, same honesty tooltip as the panel button:
  `"Appends a field note to the session log; no physical sample is claimed."`.
- starts disabled.
- connect `clicked` to `MainWindow::collectSample`.

In `openSampleLog()`:

- on load failure: disable the toolbar button, tooltip unchanged.
- on success: enable it only when `m_primaryTargetId` is non-empty; set tooltip
  `"Collect a field note at <id>."` when enabled. Do not claim a physical
  sample.
- the panel's own `setCollectTarget` behaviour stays exactly as is.

## 2. Reveal after append

Add `void SidePanel::revealLatestSample();` that calls
`m_scroll->ensureWidgetVisible(m_latestSampleLabel, 0, 24)` when both exist
(null-safe, no-op otherwise).

In `MainWindow::collectSample()` after `setLatestSample(...)` and before
`updateStatus()`, call `m_sidePanel->revealLatestSample()`.

Change the normal status suffix from:

```cpp
text += QStringLiteral(" · SAMPLE %1").arg(id);
```

to:

```cpp
text += QStringLiteral(" · %1 LOGGED").arg(id);
```

so it reads `SAMPLE-3-1 LOGGED`, not `SAMPLE SAMPLE-3-1`.

## 3. Test

In `tests/test_ui.cpp::collectsSampleIntoSessionLog()`:

- after finding the panel `QPushButton`, also find
  `collectSampleToolbarButton` as `QToolButton`, assert it exists and is
  enabled;
- use the **toolbar** button for the first click (this proves the always-
  visible path works); assert sample ID `SAMPLE-3-1`, label contains the
  geotag/orientation, and status contains `SAMPLE-3-1 LOGGED`;
- in the second `MainWindow`, keep using the panel button for the second
  sample (`SAMPLE-3-2`), preserving the existing end-to-end coverage.
- do not weaken existing assertions.

## Verification

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Report exact totals. No visual-check claim.
