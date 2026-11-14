# Brief 06b — M6 acceptance tightening

Small follow-up on 06a. The M6 model/log/UI is in place and 10/10 tests pass,
but the visible sample line must show the **coordinates and up-vector**, and
the log must reject malformed IDs and empty provenance fields. No new feature.

## Files you may change

- `src/app/SampleLog.cpp` (validation / ID format only)
- `src/ui/MainWindow.cpp` (latest-sample summary only)
- `src/ui/SidePanel.h` (remove the duplicate `public:` label only)
- `tests/test_samplelog.cpp`
- `tests/test_ui.cpp`

Do not touch `docs/`, `assets/`, `data/`, `tools/`, `logs/`, root `*.md`, or
M4/M5 files (`MissionState`, `TraversePlanner`, scenario state machine).

## 1. `SampleLog::validate` — strict identity and provenance

Add these checks before returning true:

- ID must match `^SAMPLE-[0-9]+-[0-9]+$`. Use `QRegularExpression` or manual
  parsing. The first number must equal `record.sol`, the second must be `>= 1`.
  Error must mention the malformed ID and expected `SAMPLE-<sol>-<n>`.
- `orientationMethod`, `orientationSource`, `positionSource`, and
  `measurement` must be non-empty after `trimmed()`.
- `upUp` must be `> 0.0` after the unit-length check (an ENU normal cannot
  point down).
- `compositionMeasured` must be `false`; if true, fail with
  `"composition_measured must be false for a terrain-only field note"`.
  This keeps the M6 honesty rule enforced in the file format.
- Keep all existing checks. Keep the JSON schema unchanged.

`SampleLog::nextId` may stay as is; loading now guarantees valid IDs.

## 2. `MainWindow::collectSample` — visible geotag + orientation

The latest-sample label currently shows only `SAMPLE-3-1 · Pahrump Hills ·
slope 2.9° terrain context · 1 logged`. Replace the summary passed to
`SidePanel::setLatestSample` with a multi-line summary that includes:

```text
<targetId> <targetName>
E <easting, 2 dp> m · N <northing, 2 dp> m
up (<upEast, 2 dp>, <upNorth, 2 dp>, <upUp, 2 dp>) · slope <slope, 1 dp>°
<phenomenon>
```

Use ASCII `E`/`N`/`up`/`slope` and the actual record values, never hard-coded
coordinates. Do not change `SidePanel::setLatestSample`'s signature; it already
prepends the ID and appends `" · <n> logged"`. Ensure the label is word-wrapped
(it is) and that the panel scrolls if the note is tall.

## 3. `SidePanel.h` cleanup

There are two consecutive `public:` labels around the M6 signal block. Remove
the redundant second one only; do not reorder or rename anything.

## 4. Tests

`tests/test_samplelog.cpp` — extend `rejectsInvalidRecord()` (or add a new
slot) with:

- `id = "SAMPLE-3-x"` → rejected and no file;
- `id = "SAMPLE-4-2"` with `sol = 3` → rejected;
- empty `measurement` → rejected;
- empty `positionSource` → rejected;
- `compositionMeasured = true` → rejected.

`tests/test_ui.cpp` — in `collectsSampleIntoSessionLog()` after the first
click:

```cpp
auto *latest = w.sidePanel()->findChild<QLabel *>(
    QStringLiteral("latestSampleLabel"));
QVERIFY(latest);
QVERIFY2(latest->text().contains(QStringLiteral("E ")), ...);
QVERIFY2(latest->text().contains(QStringLiteral("N ")), ...);
QVERIFY2(latest->text().contains(QStringLiteral("up (")), ...);
QVERIFY2(latest->text().contains(QStringLiteral("slope ")), ...);
QVERIFY2(latest->text().contains(QStringLiteral("SAMPLE-3-1")), ...);
```

Do not weaken existing assertions.

## Verification

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Report exact totals and test names. No visual-check claim.
