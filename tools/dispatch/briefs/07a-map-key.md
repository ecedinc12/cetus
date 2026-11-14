# Brief 07a — explain the map overlays and the rotated HiRISE footprint

The merged M4/M5 build shows coloured blocks on the map and a visibly tilted
image strip. Neither is explained in the UI. Add a compact **MAP KEY / DATA
NOTE** to the bottom legend and give **unknown hazard cells their own colour**
instead of the no-go red.

This is presentation only. Do not change the hazard thresholds, the routing,
the scenario, or any measured value.

## Files you may change

- `src/analysis/HazardMap.cpp`
- `src/ui/LayerLegend.h`, `src/ui/LayerLegend.cpp`
- `tests/test_analysis.cpp`
- `tests/test_layerlegend.cpp`

Do not touch `docs/`, `assets/`, `data/`, `tools/`, `logs/`, root `*.md`,
`MainWindow`, `MapView`, `SidePanel`, or any sample-logging file.

## 1. Unknown hazard cells get a distinct colour

In `HazardMap::hazardImage` the current branch is:

```cpp
if (!std::isfinite(slope) || slope > kNoGoSlopeDeg)
    line[x] = qRgba(220, 30, 20, 170);
else if (slope >= 10.0)
    line[x] = qRgba(255, 170, 0, 120);
```

Split the unknown case out. Keep the existing no-go red and caution amber
values exactly as they are, and use **purple** for unknown/no-data:

```cpp
constexpr QRgb kUnknown = qRgba(155, 89, 192, 170);
```

So:

- `!std::isfinite(slope)` -> purple `kUnknown` (unknown / no DEM data);
- `slope > kNoGoSlopeDeg` -> red `220,30,20,170` (known steep no-go);
- `slope >= 10.0` -> amber `255,170,0,120` (caution, still traversable).

Unknown cells remain no-go for routing (`isNoGo()` must not change). Add a
short comment naming the three colours: red = known no-go, purple = unknown
no-data (still no-go), amber = caution 10–20°.

## 2. Add a MAP KEY / DATA NOTE column to `LayerLegend`

Keep every existing layer row, object name (`layerToggle_<id>`,
`layerDetail_<id>`), signal and accessor working exactly as before. Restructure
the widget body only:

- A horizontal top-level layout.
- Left column: the existing `LAYERS` title, hint, layer rows and stretch.
- A thin vertical separator (`QFrame::VLine`).
- Right column: a `MAP KEY` section, compact and word-wrapped, with a fixed
  minimum width around 360 px and a maximum around 460 px so it does not push
  the layer rows off screen at 1280×800.

Add to `LayerLegend`:

```cpp
QString mapKeyText() const;   // test hook, returns the note's text
```

Store the note in `QLabel *m_mapKeyNote` with object name `mapKeyNote`.
Use `Qt::RichText`, `setWordWrap(true)` and a small font (about 11–12 px).
The text must name, at minimum:

- **HAZARD** — "derived 10 m grid from the HiRISE slope layer" (state that it
  is derived, not a separate NASA product);
- red **NO-GO** — slope > 20°;
- purple **UNKNOWN** — no DEM data; treated as no-go;
- amber **CAUTION** — slope 10–20° (still traversable);
- cyan **ROUTE** — primary leg; dashed cyan **PLAN** — later legs of the
  multi-target plan;
- green **START**, amber **TARGET**, orange **NEXT** marker;
- a **DATA NOTE**: the HiRISE strip is rotated inside the north-up Mars map
  grid; the black wedges are no data, not terrain; the north arrow is
  map-grid north.

Do not claim a separate measured data kind. This is a key for overlays derived
from the already-documented HiRISE optical/DEM/slope layers.

## 3. Tests

In `tests/test_layerlegend.cpp` add a slot such as
`mapKeyExplainsOverlaysAndFootprint()`:

- build a `LayerLegend` (with or without a registry; the key is static);
- `mapKeyText()` is non-empty and contains, case-insensitively:
  `NO-GO`, `UNKNOWN`, `CAUTION`, `ROUTE`, `rotated`, `north-up`, `no data`,
  and `20`;
- the label with object name `mapKeyNote` exists and is the same text;
- existing layer-row assertions still pass.

In `tests/test_analysis.cpp` add a slot such as
`hazardImageMarksUnknownDistinctly()`:

- open the prepared slope layer and build the real hazard grid (reuse the
  existing helper / skip pattern);
- skip if `stats().unknownCount == 0`;
- render `hazardImage(QSize(320, 640))` and scan for at least one exact
  unknown purple pixel (`qRgba(155,89,192,170)`) and at least one exact no-go
  red pixel (`qRgba(220,30,20,170)`); if the real grid has no known no-go
  pixel at that resolution, skip that half rather than inventing data;
- keep `hazardImageRowsAreNorthUp()` passing unchanged.

## Verification

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Report exact test totals. No screenshot or visual-check claim.
