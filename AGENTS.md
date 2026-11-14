# Cetus — agent instructions

A native Qt 6 / C++ desktop tool for a **Mars astronaut** conducting surface science.
Built for a hackathon; scope is deliberately small and demo-driven.

Read this file before writing code. If a task conflicts with it, ask first.

## Agent workflow

- Use **DeepSeek V4.1 Flash subagents** for coding tasks, with **thinking disabled**.
  Dispatch them through `tools/dispatch/worker.sh` (model and thinking are
  pinned there):

  ```bash
  tools/dispatch/worker.sh <name> tools/dispatch/briefs/<brief>.md &
  tools/dispatch/watch.sh <name>
  ```

  Workers write `src/`, `tests/` and `CMakeLists.txt` only. They must not touch
  `docs/`, `data/`, `assets/`, `tools/`, `logs/`, or root Markdown; the harness
  audits the diff and fails with exit 78 otherwise.
- **Ask the prompter for any data you need** before proceeding. Do not invent
  data, coordinates, product IDs, or thresholds when the prompter can supply
  them.

---

## 1. The brief (what is actually required)

Produce a **map / tool / interface** an astronaut could use *during* a surface
exploration mission, using NASA data and tools.

Hard requirements:

1. A usable map/tool/interface for an astronaut in the field.
2. Real NASA data (name the sources in the UI and the README).
3. A stated scientific goal and a chosen landing region.
4. **At least three kinds of data** combined (optical, thermal, spectral,
   topographic, geophysical, atmospheric, radiation, chemical).
5. Information that helps the crew move **safely** (safety is the top priority).
6. Account for **spacesuit mobility** (slow, gloved, consumable-limited, loaded).
7. A **route map** that reaches the science targets while **avoiding obstacles**
   such as large boulder fields / steep slopes.

Not required: physics simulation, real-time telemetry, 3D first-person view,
full-planet coverage.

---

## 2. Scope decisions (locked — do not silently expand)

- **Platform:** native desktop app, Qt 6 + C++17. No web app, no TypeScript,
  no Electron, no Python in the shipped product.
- **Region:** ONE area only — the **Mount Sharp / Gale Crater** region, chosen
  because the science goal is **boxwork vein formations**. Do not attempt
  full-Mars high-resolution coverage. A low-res global context basemap is
  optional and must not block the MVP.
- **MVP UI:** map-centric. Large map view + side panel (goal, next target,
  route, consumables, warnings) + status bar (sol, time, tau, weather,
  radiation). A static "helmet view" panel may be added later; a 3D
  first-person renderer is out of scope.
- **Data prep is offline.** Rasters are cropped/reprojected/tiled ahead of time
  with GDAL and shipped as local assets. The app must not depend on a network
  server or live API at demo time.
- **Route:** science-value-weighted traverse with hard hazard and walkback
  constraints. A plain shortest-path algorithm is not sufficient and not the
  deliverable on its own.

---

## 3. Science concept

- Goal: characterize **boxwork / fracture-filling mineral veins** (sulfate and
  hydrated phases) to reconstruct past fluid chemistry and habitability.
- Why it matters: veins record fluid composition, multiple flow episodes,
  subsurface habitability, and biosignature preservation potential.
- Targets should carry *why* they matter, the observation protocol, and the
  estimated time-on-target — not just a pin on a map.

**Fact-check before pinning a site.** The brief cites Curiosity boxwork work in
2025; the well-known "Garden City" boxwork was observed around 2015. Confirm the
actual site/coordinates from a NASA source and record the citation in
`docs/data-sources.md`. Never invent coordinates.

---

## 4. Data layers (target set)

Pick from these; ship at least three, fused:

| Kind | Source | Use |
|---|---|---|
| Optical | HiRISE (~0.25–0.5 m/px), CTX (~6 m/px) | base map, vein geometry, boulders |
| Topographic | MOLA, HiRISE/CTX stereo DEM | slope, walkback, relief |
| Spectral | CRISM (+ CRISM/RELAB reference spectra) | sulfate/hydrated mineral detection |
| Thermal | THEMIS day/night | rock abundance, dust, trafficability |
| Atmospheric | MARCI / MCS / tau estimates | dust, wind, visibility |
| Radiation | MSL RAD / MAVEN context | crew safety limits |
| Chemical | ChemCam/APXS/CheMin (Curiosity) | ground truth at visited targets |

Rules:

- Every layer in the UI must display its **source, resolution, and units**.
- Distinguish **NASA-verified** data from **synthetic/derived/demo** fixtures in
  both the UI and the file names. Never let a fixture masquerade as real data.
- Missing raster values mean **unknown**, never "safe to traverse".

---

## 5. Mars geodesy

- Never assume Earth WGS84 or Web Mercator. Mars uses its own CRS
  (IAU Mars 2015 body, e.g. `IAU_2015:49900` / a Mars equirectangular or
  south-polar stereographic projection). Use **GDAL + PROJ** for all
  reprojection and always carry the CRS explicitly.
- Keep the native planetary CRS through the pipeline; reproject only at the
  final display step, and document the transform.

---

## 6. Architecture (keep it boring)

```
src/
  main.cpp              entry point, QApplication
  app/                  application services, mission state
  data/                 dataset registry, raster loading (GDAL), tiles
  gis/                  planetary CRS, coordinate transforms, terrain
  analysis/             slope, hazard, science-value scoring
  routing/              traverse solver (constraints + objective)
  ui/                   Qt widgets / QML views, suit-mode theme
assets/                 prepared tiles, spectra libraries, icons
data/raw/               downloaded NASA products (gitignored, large)
data/processed/         GDAL outputs used by the app (gitignored)
docs/                   data sources, decisions, demo script
tests/                  unit tests + fixture rasters
```

- CMake, `CMAKE_CXX_STANDARD 17`, `AUTOMOC` on.
- Prefer Qt Widgets or QML — pick one and stay consistent; do not mix for the
  same screen.
- Keep GDAL access behind one module; the UI must never touch GDAL directly.
- No global mutable state. Mission state lives in one object, passed explicitly.
- Fail loudly on missing/corrupt data; never silently substitute a default.

---

## 7. UI rules (astronaut, not analyst)

- One **primary recommendation** at a time ("go here next"), with drill-down.
- High contrast, large hit targets, glove-friendly; usable on a tablet frame.
- Hazard-first: no-go areas and warnings must be visually obvious.
- Always show consumables and walkback status next to the route.
- No live-Earth assumptions: the app must work fully offline.
- It should read as **field equipment**, not a mission-control dashboard.

---

## 8. Build & run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/cetus
```

Toolchain available in this environment (verified):
CMake 4.4.5, GCC 16.2.1, Qt 6.12.0, GDAL 3.13.3, PROJ 9.9.0.

Add tests as `ctest` targets. Document every command you actually ran; never
claim validation you did not execute.

---

## 9. Conventions

- C++17. Use Qt style consistently: `PascalCase` types, `camelCase` methods,
  `m_` member prefix.
- Keep commits scoped; no drive-by refactors.
- Any hard-coded path, coordinate, or threshold must be named and commented.
- Update `docs/data-sources.md` whenever a dataset is added.
- Do not fabricate: no invented coordinates, metadata, spectra, or observations.

---

## 10. Definition of done (MVP)

- [ ] App launches offline and shows a Mars basemap for the chosen region.
- [ ] >= 3 data kinds are visible and toggleable, each with source/units shown.
- [ ] Science targets are ranked and explain *why* they matter.
- [ ] A route to the next target is drawn with hazards avoided and consumables
      plus walkback shown.
- [ ] A scripted "dust rises / comms closes" state change visibly re-ranks
      targets and reroutes.
- [ ] README + demo script + data-source citations are complete.

If a feature does not help the demo tell the story, cut it.
