# Cetus — build plan

Companion to `AGENTS.md`. This file is the executable handoff: milestones,
exact data, commands, and numbers. If this plan and the brief disagree, the
brief wins; if this plan and `AGENTS.md` disagree, `AGENTS.md` wins.

Timeboxes are relative (`H0` = start). Adjust the calendar, keep the order.

---

## 0. Ground rules

- Everything runs **offline** at demo time. Network only during data prep.
- No fabricated coordinates or data. Anything not from NASA is labeled
  `SYNTHETIC` in the filename and in the UI.
- Every number below that is a design choice (speed, thresholds) is marked
  **[tunable]**, not a NASA value.
- Keep the app runnable at every milestone. Never leave the tree non-building.

---

## 1. Milestones

| # | When | Deliverable | Acceptance |
|---|---|---|---|
| M0 | H0–H1 | CMake scaffold builds; window opens; empty map canvas | `cmake --build build && ./build/cetus` shows a window |
| M1 | H1–H3 | Prepared local rasters load into a zoom/pan canvas | HiRISE/CTX crop visible, correct orientation, no Earth CRS |
| M2 | H3–H6 | 3+ toggleable layers with source/units overlay | Legend names each source; toggles work |
| M3 | H6–H10 | Targets ranked; route drawn; hazard polys | Route avoids no-go areas; side panel shows "why" |
| M4 | H10–H13 | Consumables + walkback + suit speed | Panel updates when route/target changes |
| M5 | H13–H16 | Scenario state machine (tau rise) re-ranks + reroutes | One control triggers visible replan |
| M6 | H16–H18 | Sample logging / field note + geotag | Sample appears with ID, orientation, coords |
| M7 | H18–H20 | Polish, README, citations, demo rehearsal | Demo runs start-to-finish without a crash |

Cut from the **end** if time runs out, not the middle.

---

## 2. Scaffold (M0)

Create exactly this; do not add modules speculatively.

```
CMakeLists.txt
src/main.cpp
src/app/MissionState.{h,cpp}
src/data/DatasetRegistry.{h,cpp}
src/data/RasterLayer.{h,cpp}
src/gis/PlanetaryCrs.{h,cpp}
src/analysis/HazardMap.{h,cpp}
src/analysis/TargetRanking.{h,cpp}
src/routing/TraversePlanner.{h,cpp}
src/ui/MainWindow.{h,cpp}
src/ui/MapView.{h,cpp}
src/ui/SidePanel.{h,cpp}
assets/layers/.gitkeep
data/raw/.gitkeep
docs/data-sources.md
tests/CMakeLists.txt
```

`CMakeLists.txt` essentials:

```cmake
cmake_minimum_required(VERSION 3.20)
project(cetus VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(GDAL REQUIRED)
add_executable(cetus src/main.cpp ...)
target_link_libraries(cetus PRIVATE Qt6::Widgets GDAL::GDAL)
enable_testing()
add_subdirectory(tests)
```

**Decision (closes the AGENTS ambiguity): use Qt Widgets.** The map is a custom
`QWidget` painted with `QPainter` from GDAL-read buffers (or a `QGraphicsView`).
Do not mix QML into the same screen.

---

## 3. Data acquisition (M1)

### 3.1 Choose the exact site

1. Start from the Curiosity landing area, **Bradbury Landing / Gale Crater**
   (published ~4.59°S, 137.44°E — confirm, do not trust this line blindly).
2. Locate the boxwork/vein site from a NASA source:
   - Curiosity raw images + Analyst's Notebook (`an.rsl.wustl.edu`)
   - Published coordinates for "Garden City" or the newer 2025 boxwork site
3. Record: site name, lat/lon, citation, retrieval date → `docs/data-sources.md`.
4. Define a **bounding box ~10–20 km** around the site. Store it once in
   `src/data/SiteConfig.h` as named constants.

### 3.2 Products to fetch

Use **Mars ODE** (`ode.rsl.wustl.edu/mars`) and the HiRISE/CTX/CRISM catalogs.
Search by bounding box, not by guessing product IDs.

| Kind | Product | Target resolution | Notes |
|---|---|---|---|
| Optical (hi) | HiRISE ortho, 1–3 products | 0.25–0.5 m/px | color or enhanced color |
| Optical (context) | CTX mosaic, 1–2 products | ~6 m/px | wide base |
| Topographic | HiRISE or CTX stereo DEM | 1–20 m/px | must match ortho extent |
| Spectral | CRISM targeted observation over site | ~18–36 m/px | check coverage exists |
| Thermal (optional) | THEMIS day IR mosaic | ~100 m/px | global product, crop |
| Radiation (optional) | MSL RAD table / MAVEN context | n/a | static chart, not a raster |

Save raw downloads to `data/raw/` (gitignored) with original product IDs in the
name. **Record every product ID in `docs/data-sources.md`.**

### 3.3 Fallback

If a product is unavailable in time:

- Prefer a **coarser real product** over a fake one.
- If nothing real exists, generate a `SYNTHETIC` fixture
  (`gdal_create` + a noise/slope script) and label it loudly in UI + docs.
- Never silently substitute synthetic for real.

---

## 4. Processing pipeline (M1)

Run offline, output to `assets/layers/`. Use Mars body radius
**3396190 m** and an equirectangular Mars CRS, e.g.
`+proj=eqc +lat_ts=0 +lon_0=137.44 +R=3396190 +units=m +no_defs`.
Never WGS84.

```bash
# 1. Crop to site, reproject to Mars equirectangular, write COG
gdal_translate -of COG \
  -projwin <ulx> <uly> <lrx> <lry> \
  -a_srs "+proj=eqc +lat_ts=0 +lon_0=137.44 +R=3396190 +units=m +no_defs" \
  data/raw/<product>.tif assets/layers/<name>_opt_hi.tif

# 2. Build a display pyramid
gdaladdo -r average assets/layers/<name>_opt_hi.tif 2 4 8 16

# 3. Derive slope from the DEM (degrees)
gdaldem slope -alg Horn -p assets/layers/<name>_dem.tif assets/layers/<name>_slope.tif

# 4. Derive a hillshade for context
gdaldem hillshade -z 2 assets/layers/<name>_dem.tif assets/layers/<name>_hillshade.tif
```

Naming: `<site>_<kind>_<source>.tif`, e.g. `gale_opt_hirise.tif`,
`gale_dem_ctx.tif`, `gale_spec_crism.tif`.

If CRISM is present, precompute a simple band-index raster (e.g. hydrated-silica
or sulfate index) so the UI can show a single "mineral" layer without doing
spectroscopy at runtime. Document the index formula in `docs/data-sources.md`.

---

## 5. Core model (M2–M4)

### 5.1 Mission state

One `MissionState` object, owned by the app, passed by reference:

```
sol, clock, tau, wind, temp, radiation
crew_x, crew_y
o2_pct, power_pct, water_pct
elapsed_min, time_budget_min
layers_on[]
targets[]
route[]
```

Starts from a `Scenario` struct (see §6) so the demo is deterministic.

### 5.2 Science targets

Each target carries:

```
id, name, x, y
value (0–100)      // priority × confidence × uniqueness
phenomenon         // "boxwork vein"
why                // one sentence, shown in UI
observation        // protocol / instrument
minutes_on_target  // [tunable]
```

Rank by `value / (travel_time + minutes_on_target)` — a **rate**, not raw value.

### 5.3 Hazards and cost

Grid cost per cell:

```
cost = distance / suit_speed
     + hazard_penalty(slope, rock_abundance, unknown_data)
```

- `suit_speed` default **1.2 km/h** flat **[tunable]**; degrade with slope.
- `slope > 20°` → no-go **[tunable]**
- high rock abundance / boulder field → strong penalty (derived from optical
  texture or thermal proxy; if unavailable, mark unknown and penalize lightly)
- missing data → treat as **unknown**, apply penalty, never as free

### 5.4 Walkback (hard constraint)

From every route point, remaining consumable time must cover:

```
time_to_nearest_safe_haven(1.2 × path_time) + 15 min reserve
```

`safe_haven` = lander or a designated point. If a plan violates this, it is
invalid — drop the farthest target and replan.

### 5.5 Solver

Keep it simple and explainable:

1. Build a grid graph from the DEM/slope raster.
2. A* from crew → each candidate target (hazard-weighted).
3. Pick the target with the best value rate that satisfies walkback.
4. Greedy repeat until time budget or walkback fails.
5. Optional 2-opt pass over the target order.

No external solver library. ~300–500 lines.

---

## 6. Scenario state machine (M5)

Deterministic, one control in the header ("Advance scenario"):

| Step | Change | Expected behavior |
|---|---|---|
| S0 | tau 0.6, all targets valid | recommends T2, full route |
| S1 | tau 0.8 | warning appears, same plan |
| S2 | tau 1.1 | drops farthest/lowest-value target, reroutes, banner explains |
| S3 | comms window closes | badge "offline until +40 min", no functional change |
| S4 | power < 35% | forces return to haven, science disabled |

Thresholds live in one header, not scattered.

---

## 7. UI (M0–M7)

Widgets layout (desktop, resizable):

```
┌───────────────────────────────────────────────────────────────┐
│ SOL · TIME · TAU · WIND · TEMP · RAD        [Advance scenario]│
├──────────────────────────────────────┬────────────────────────┤
│                                      │ SCIENCE GOAL           │
│              MapView                 │ NEXT TARGET            │
│        (rasters + route + hazards)   │ why / protocol / time  │
│                                      │ ROUTE (dist/min/slope) │
│                                      │ CONSUMABLES            │
│                                      │ WARNINGS               │
├──────────────────────────────────────┴────────────────────────┤
│ Layer toggles with source + units                             │
└───────────────────────────────────────────────────────────────┘
```

Rules:
- One primary recommendation, visually dominant.
- Warnings always visible, never a modal that blocks the map.
- Every layer label includes source + resolution + units.
- Suit mode = larger fonts, higher contrast, no tiny controls.

---

## 8. Sample logging (M6)

On "Collect sample" at a target:

- assign `SAMPLE-<sol>-<n>`
- record x/y, target id, orientation (up-vector from DEM), timestamp
- link the auto-generated field note (phenomenon, why, measurement)
- append to a session log file (JSON or CSV) with provenance

No chain-of-custody simulation beyond this; keep it honest.

---

## 9. Tests

`ctest` with:

- CRS round-trip: project a known point and back, assert tolerance.
- Slope: synthetic ramp DEM → expected degrees.
- Cost/A*: small hand-built grid → known shortest path.
- Ranking: two targets with known values/times → expected order.
- Walkback: plan that must be rejected is rejected.
- Scenario: S0→S4 produces expected target counts.

Fixtures in `tests/fixtures/`, generated by a `create_fixtures` target, clearly
named synthetic.

---

## 10. Demo rehearse (M7)

Follow `docs/demo-script.md`. Time it. Kill anything that costs seconds.
Freeze the dataset — no network, no downloads during rehearsal.

---

## 11. Risks

| Risk | Mitigation |
|---|---|
| HiRISE/CRISM download slow or missing | start downloads at H0, in parallel with scaffold; fall back to CTX + THEMIS |
| Qt/GDAL version mismatch | verified: CMake 4.4.5, GCC 16.2.1, Qt 6.12.0, GDAL 3.13.3, PROJ 9.9.0 |
| Raster too big for memory | crop first, use COG + overviews, load at display resolution |
| Wrong Mars CRS/orientation | assert CRS on load; render a known landmark and eyeball it |
| Scope creep (3D, POV, full planet) | AGENTS.md §2 is locked |
| Demo crash from live replan | scenario is deterministic and pre-tested |
