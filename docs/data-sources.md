# Cetus — data sources and provenance

All data below is **real NASA data**. No synthetic raster is used. Raw downloads
live in `data/raw/` (gitignored); the prepared, app-ready rasters live in
`assets/layers/` (gitignored as large binaries) and are listed in the manifest
section at the end.

The app is offline at demo time. The URLs here are only for provenance and
re-prep.

---

## 1. Site

**Gale Crater — Pahrump Hills / Garden City vein complex, Mars.**

- Science goal: characterise fracture-filling (boxwork) sulfate and hydrated
  mineral veins to reconstruct past fluid chemistry and habitability.
- Site anchor (NASA-published observation centre, **not invented**):
  **4.695°S, 137.381°E** (planetocentric).
  Source: HiRISE observation page for `ESP_040770_1755`,
  <https://www.uahirise.org/ESP_040770_1755>.
  The same page (acquired 08 April 2015, local Mars time 14:08) describes the
  rover “parked over dark sand in a valley bounded by light-toned rock
  outcrops” of the **Pahrump member** of the Murray Formation, “a suite of
  sandstones, siltstones, and calcium sulfate veins” at the base of Mount Sharp.
- The Garden City vein complex is part of this Pahrump outcrop belt. NASA's
  PIA19161 (“Prominent Veins at Garden City”, Mastcam, 18 March 2015) and
  PIA19921 (“Veiny Garden City site/surroundings”) document the target:
  <https://photojournal.jpl.nasa.gov/catalog/PIA19161>,
  <https://photojournal.jpl.nasa.gov/catalog/PIA19921>.
- **Target pins come from NASA Mars Trek, not from guessed coordinates.**
  M3 uses three named Curiosity waypoints from the Mars Trek
  `curiosity_waypoints` service (Pahrump Hills, Marias Pass, Vera Rubin Ridge)
  and an EVA start point from a vertex of the Mars Trek `curiosity_path`
  service. The waypoint/path vertices are published in Mars body
  longitude/latitude; the app stores them in the site Mars equirectangular
  CRS after a documented transform (section 2.4). No coordinate is invented or
  digitised by eye.
- Site bounding box (exactly the HiRISE DTM/ortho footprint, from the product
  labels): **137.33518211°E to 137.44270087°E, 4.79220274°S to 4.58331793°S**.
  That is 6.37 km east–west by 12.38 km north–south at 1.01 m/px. The PLAN.md
  guideline was 10–20 km; this real stereo footprint is smaller but complete
  and fully covered by 1 m data. A wider CTX context layer is a later addition,
  not a substitute for this scene.

---

## 2. Primary rasters

### 2.1 Optical — HiRISE orthoimage

| Field | Value |
|---|---|
| Product ID | `ESP_040770_1755_RED_C_01_ORTHO` |
| Source | NASA/JPL/University of Arizona, MRO HiRISE |
| Observation | `ESP_040770_1755`, acquired 2015-04-08 |
| Native map scale | 1.0117630165416 m/px |
| Image size | 6300 × 12238 px (single band, 8-bit) |
| Projection | Mars equirectangular, planetocentric, `CENTER_LONGITUDE = 137.39°`, `A_AXIS_RADIUS = 3396.19 km` |
| Extent | 137.33518211–137.44270087°E, 4.58331793–4.79220274°S |
| Local file | `data/raw/ESP_040770_1755_RED_C_01_ORTHO.JP2` + `.LBL` |
| Prepared layer | `assets/layers/gale_hirise_ortho_1m.tif` (COG, Deflate) |
| NASA page | <https://www.uahirise.org/ESP_040770_1755> |
| PDS base | <https://hirise-pds.lpl.arizona.edu/PDS/DTM/ESP/ORB_040700_040799/ESP_040770_1755_ESP_039280_1755/> |

The orthoimage is orthorectified against the stereo DTM below, so optical and
topography share a grid and a CRS. The equirectangular map grid is north-up;
the scene is resampled into that grid. The observation footprint itself is a
rotated rectangle inside the grid, so the JP2 has black triangular no-data
wedges at the swath edges. They are *not* terrain and must never be treated as
traversable data. The tilted strip in the app is that real footprint, not a
rendering rotation; the north arrow refers to the map grid.

### 2.2 Topographic — HiRISE stereo DTM

| Field | Value |
|---|---|
| Product ID | `DTEEC_040770_1755_039280_1755_U01` |
| Source | NASA/JPL/University of Arizona / USGS Astrogeology, MRO HiRISE stereo |
| Stereo pair | `ESP_040770_1755` + `ESP_039280_1755` (the latter is “Curiosity Rover at Pahrump Hills”) |
| Native post spacing | 1 m |
| Image size | 6300 × 12238 px, 32-bit float |
| Projection | Mars equirectangular, `CENTER_LONGITUDE = 137.39°`, `A_AXIS_RADIUS = 3396.19 km`, 1.011763 m/px |
| Elevation range | −4503.92 m to −3472.86 m above the Mars areoid |
| Valid data | 73.91% (the rest is `MISSING_CONSTANT` / nodata) |
| Accuracy (README) | RMS x 4.58 m, RMS y 0.024 m, RMS z 1.12 m, total RMS 3.10 m; mean difference from MOLA 0.19 m |
| Local file | `data/raw/DTEEC_040770_1755_039280_1755_U01.IMG` |
| Prepared layer | `assets/layers/gale_hirise_dem_4m.tif` (resampled to 4 m/px, COG) |
| PDS base | <https://hirise-pds.lpl.arizona.edu/PDS/DTM/ESP/ORB_040700_040799/ESP_040770_1755_ESP_039280_1755/> |
| Product README | `README_DTEEC_040770_1755_039280_1755_U01.TXT` in the `EXTRAS/DTM/...` directory |

The raw DTM is kept at 1 m for analysis; the app-ready copy is 4 m/px to keep
memory and route-grid costs sane. **Missing DTM cells mean “unknown”, never
“safe to traverse”.** The DTM is not a certified navigation surface.

### 2.3 Derived layers

| Layer | File | Units | Formula / command |
|---|---|---|---|
| Slope | `assets/layers/gale_hirise_slope_4m.tif` | degrees | `gdaldem slope -alg Horn` on the 4 m DEM (**no `-p`**) |
| Hillshade | `assets/layers/gale_hirise_hillshade_4m.tif` | unitless 0–255 | `gdaldem hillshade -z 2 -az 315 -alt 45` on the 4 m DEM |

The hillshade illumination (azimuth 315°, altitude 45°, z-factor 2) is a
**display choice**, not a measured solar position. The HiRISE page gives a
solar incidence angle of 33° for the source image; if a physically consistent
shade is needed later, use the acquisition geometry. Slope is computed at 4 m
post spacing, so it smooths metre-scale boulders; boulder-scale hazard will
need the 1 m DTM or optical texture in M3.


### 2.4 Science-target waypoints — NASA Mars Trek

M3's crew start and three science targets are **real NASA Mars Trek features**,
not synthetic fixtures:

| App ID | Mars Trek feature | Mars Trek FID | Longitude (°E) | Latitude (°N) |
|---|---|---|---|---|
| `START` | `curiosity_path` vertex 6300 | — | 137.3772897332912 | −4.660079490499737 |
| `T1` | Pahrump Hills | 5 | 137.37724353716703 | −4.66241193815176 |
| `T2` | Marias Pass | 6 | 137.37140256639952 | −4.667938835464834 |
| `T3` | Vera Rubin Ridge | 11 | 137.36832 | −4.72053 |

Services (ArcGIS REST, queried 2025-11-14):

```bash
# All waypoints, geometry in Mars body lon/lat (wkid 104905)
curl -sS 'https://trek.nasa.gov/mars/trekarcgis/rest/services/curiosity_waypoints/MapServer/0/query?where=1%3D1&outFields=*&returnGeometry=true&outSR=104905&f=json'

# Full Curiosity traverse polyline; vertex 6300 is the EVA start
curl -sS 'https://trek.nasa.gov/mars/trekarcgis/rest/services/curiosity_path/MapServer/0/query?where=1%3D1&outFields=*&returnGeometry=true&outSR=104905&f=json'
```

Feature text used for the target "why" lines (fetched from Mars Trek, not
paraphrased from other sites):

- <https://trek.nasa.gov/mars/jpl/assets/features/curiosity/pahrump_hills.html>
- <https://trek.nasa.gov/mars/jpl/assets/features/curiosity/marias_pass.html>
- <https://trek.nasa.gov/mars/jpl/assets/features/curiosity/vera_rubin_ridge.html>

The app file is `assets/scenario/m3_scenario.json`. It stores both the
published lon/lat and the site-CRS easting/northing. The transform used was:

```bash
# Example: Pahrump Hills. Same string as the raster CRS.
echo '137.37724353716703 -4.66241193815176' |   gdaltransform -s_srs '+proj=longlat +R=3396190 +no_defs'                 -t_srs '+proj=eqc +lat_ts=0 +lon_0=137.39 +R=3396190 +units=m +no_defs'
# -> -756.135485790287 -276363.057354002
```

**Not from NASA:** the `science_value` (0–1) and `minutes_on_target` fields in
the scenario file are design choices used to rank targets (`[tunable]`). They
are labelled as scenario values in the file and are not presented as measured
science. The target pin *locations* are NASA Mars Trek data.

Sampled 4 m slope at the pins (degrees, corrected product): START 5.58°,
Pahrump Hills 2.90°, Marias Pass 13.52°, Vera Rubin Ridge 7.49°. All three
targets are below the 20° no-go threshold at this resolution. The planner still
prints a standoff warning if a target cell is no-go or unknown at route time.

**Slope-unit correction (2025-11-14):** the first M1 slope build used
`gdaldem slope -p`, but in GDAL `-p` means **percent slope**, not degrees. Its
values reached 280.85 (% slope = 70.4°), while the metadata and UI said
degrees. That would have made the 20° no-go rule fire at ~11.3° and
over-block safe terrain. The layer was rebuilt without `-p`; new range is
0.003–70.401°, metadata `UNITS=degrees`. No raster values were clamped or
invented; this is a corrected derivation from the same DEM.

### 2.5 Traverse planning (M3)

The route is a hazard-weighted A* search over a 10 m grid built from the
corrected slope layer (`HazardMap`), not a straight line or an unweighted
shortest path.

- Grid: **638 × 1238 cells** at `10 m` (`[tunable]`). Extent and CRS come from
  the slope layer; no reprojection is applied.
- Hard constraint: slope **> 20.0°** is no-go (`[tunable]`, PLAN.md §5.3).
  No-data cells are also no-go; missing data is never treated as safe.
- Edge cost: `0.5 × (cost(a) + cost(b)) × step_metres`, where
  `cost = 1 + 2 × (slope / 20)²` for a traversable cell. Diagonal moves are
  rejected when either orthogonal neighbour is no-go or unknown.
- Crew speed: `1.2 km/h` flat (`[tunable]`, PLAN.md §5.4). The current M3
  route time does **not** yet include walkback or consumables; that is M4.
- Start/goal snapping: a pin inside a no-go/unknown cell is moved to the
  nearest safe cell within 200 m; the panel shows a standoff warning instead
  of pretending the pin is reachable. All four M3 pins are safely inside
  traversable cells, so the current standoff is 0 m.
- Ranking: `science_value / (travel_minutes + minutes_on_target)`, then
  reachable targets by decreasing value rate (travel time, then id as
  tie-breaks). Unreachable targets are listed after reachable ones.

Measured on the shipped assets (2026-11-14, offscreen):

| Item | Value |
|---|---|
| Hazard grid | 638 × 1238 cells, 10 m |
| No-go cells (> 20°) | 36,830 (4.7%) |
| Unknown cells (no slope data) | 205,428 (26.0%) |
| Maximum sampled slope | 67.34° |
| Selected target | `T1` Pahrump Hills |
| Route to `T1` | 138.3 m, 7.5 min, max slope 7.79°, standoff 0 m |
| `T2` Marias Pass | reachable, 36.0 min travel, value rate 0.0190/min |
| `T3` Vera Rubin Ridge | reachable, 211.6 min travel, value rate 0.0037/min |

The 26.0% unknown figure is dominated by the rotated HiRISE footprint's black
no-data wedges, plus DTM holes. Those areas are plotted as no-go and counted in
the panel warning; they are not evidence of safe ground. The high `T3` travel
time is the real cost of the long ridge approach under the 20° rule and the
1.2 km/h suit assumption, not a synthetic penalty.

### 2.6 Sample logging / field note (M6)

M6 adds **no new measured NASA data kind**. A collected sample is a field note
attached to one of the existing Mars Trek target pins; the terrain orientation
is derived from the shipped HiRISE DTM. There is no physical sample, no
chain-of-custody record, and no composition measurement in this prototype.

- **ID:** `SAMPLE-<sol>-<n>`, allocated per sol from the session log. A new app
  run continues from the highest existing `n` for that sol.
- **Position:** the target pin in the site Mars equirectangular CRS, plus the
  planetocentric Mars longitude/latitude from `PlanetaryCrs::eqcToLonLat`.
  The CRS (`+proj=eqc +lat_ts=0 +lon_0=137.39 +R=3396190 +units=m +no_defs`)
  is written into every record.
- **Orientation:** a Horn 3×3 surface gradient on the 4 m HiRISE DTM at the pin.
  The record stores the unit ENU up-vector, slope in degrees, downslope aspect
  (clockwise from north), and the method/source. Any nodata cell in the 3×3
  neighbourhood is a hard failure; no orientation is invented.
- **Field note:** `phenomenon`, `why` and `measurement`. `phenomenon` and `why`
  come from the real Mars Trek target metadata in
  `assets/scenario/m3_scenario.json`; `measurement` is explicitly terrain
  context only and ends `"No in-situ composition measurement was recorded."`
  The record also stores `"composition_measured": false`.
- **Session log:** JSON Lines, one object per sample, append-only. Default path
  is `<QStandardPaths::AppDataLocation>/samples.jsonl` (normally
  `~/.local/share/cetus/samples.jsonl`); set `CETUS_SAMPLE_LOG` to override it
  for a demo. A malformed existing line, duplicate ID, malformed ID, or
  invalid provenance is a loud load failure; the app does not silently repair
  or discard records.
- **Example measured at `T1` Pahrump Hills** (2026-11-14, offscreen):
  `SAMPLE-3-1`, E `-756.135485790` m, N `-276363.057354002` m, up
  `(-0.03898, 0.03231, 0.99872)`, slope `2.902°`, aspect `309.65°`. The target
  slope agrees with the corrected 4 m slope product (2.90°).

The "Collect sample" control is deliberately labelled as a field note: the
tool records where the crew stopped and what they observed. It does not claim
that a physical specimen was bagged or measured.

---

## 3. Processing actually run (M1)

Environment: GDAL 3.13.3, PROJ 9.9.0. All commands were run from the repository
root on 2025-11-14.

```bash
# Downloads (network used only during data prep)
BASE=https://hirise-pds.lpl.arizona.edu/PDS/DTM/ESP/ORB_040700_040799/ESP_040770_1755_ESP_039280_1755
curl -sSL "$BASE/ESP_040770_1755_RED_C_01_ORTHO.JP2" -o data/raw/ESP_040770_1755_RED_C_01_ORTHO.JP2
curl -sSL "$BASE/ESP_040770_1755_RED_C_01_ORTHO.LBL" -o data/raw/ESP_040770_1755_RED_C_01_ORTHO.LBL
curl -sSL "$BASE/DTEEC_040770_1755_039280_1755_U01.IMG" -o data/raw/DTEEC_040770_1755_039280_1755_U01.IMG

# Optical -> COG (CRS and geotransform are already Mars equirectangular)
gdal_translate -of COG -co COMPRESS=DEFLATE -co PREDICTOR=2 -co BIGTIFF=IF_SAFER \
  -co RESAMPLING=AVERAGE \
  -mo PRODUCT_ID=ESP_040770_1755_RED_C_01_ORTHO -mo SOURCE=HiRISE/MRO \
  -mo SOURCE_URL=https://www.uahirise.org/ESP_040770_1755 \
  -mo RESOLUTION_M_PER_PX=1.0117630165416 \
  -mo CRS=Mars_Equirectangular_lon0_137.39_R_3396190 \
  -mo UNITS=reflectance_DN_8bit -mo ACQUISITION_DATE=2015-04-08 \
  data/raw/ESP_040770_1755_RED_C_01_ORTHO.JP2 assets/layers/gale_hirise_ortho_1m.tif

# DEM -> 4 m COG (PDS driver reads the attached label)
gdal_translate -tr 4 4 -r bilinear -of COG -co COMPRESS=DEFLATE -co PREDICTOR=3 \
  -co BIGTIFF=IF_SAFER \
  -mo PRODUCT_ID=DTEEC_040770_1755_039280_1755_U01 -mo SOURCE=HiRISE_DTM/MRO \
  -mo RESOLUTION_M_PER_PX=4 -mo NATIVE_POST_SPACING_M=1 \
  -mo CRS=Mars_Equirectangular_lon0_137.39_R_3396190 \
  -mo UNITS=meters_above_Mars_areoid -mo VERTICAL_ACCURACY=1.12_m_RMS_z \
  data/raw/DTEEC_040770_1755_039280_1755_U01.IMG assets/layers/gale_hirise_dem_4m.tif

# Slope (degrees) and hillshade
gdaldem slope -alg Horn assets/layers/gale_hirise_dem_4m.tif /tmp/slope4m.tif
gdaldem hillshade -z 2 -az 315 -alt 45 assets/layers/gale_hirise_dem_4m.tif /tmp/hs4m.tif
gdal_translate -of COG -co COMPRESS=DEFLATE -co PREDICTOR=3 -co BIGTIFF=IF_SAFER \
  -mo PRODUCT_ID=DERIVED_SLOPE -mo SOURCE=derived_from_DTEEC_040770_1755_039280_1755_U01 \
  -mo METHOD=gdaldem_slope_Horn -mo RESOLUTION_M_PER_PX=4 -mo UNITS=degrees -mo NODATA=-9999 \
  /tmp/slope4m.tif assets/layers/gale_hirise_slope_4m.tif
gdal_translate -of COG -co COMPRESS=DEFLATE -co PREDICTOR=2 -co BIGTIFF=IF_SAFER \
  -mo PRODUCT_ID=DERIVED_HILLSHADE -mo SOURCE=derived_from_DTEEC_040770_1755_039280_1755_U01 \
  -mo METHOD=gdaldem_hillshade_az315_alt45_z2 -mo RESOLUTION_M_PER_PX=4 -mo UNITS=unitless \
  /tmp/hs4m.tif assets/layers/gale_hirise_hillshade_4m.tif
```

CRS string used by the app for this site (equivalent to the layer CRS):

```
+proj=eqc +lat_ts=0 +lon_0=137.39 +R=3396190 +units=m +no_defs
```

The products are delivered in the same Mars equirectangular CRS, so M1 does
**no reprojection**. If a later layer arrives in a different Mars CRS, reproject
with `gdalwarp` to this string and document the transform. Never WGS84 / Web
Mercator.

---

## 4. Caveats / non-claims

- The ortho is a single-band RED product, not colour. Do not describe it as
  colour imagery.
- Black swath-edge wedges in the ortho are no-data, not shadowed terrain.
- The DTM is 73.91% valid; the nodata value is `-3.4028227e+38`. Analyses must
  treat nodata as unknown hazard, not as flat ground.
- The 4 m slope product smooths boulders smaller than ~8 m. It is suitable for
  regional slope/walkback screening, not for metre-scale trafficability.
- Elevations are relative to the Mars areoid; they are not a global datum
  suitable for absolute navigation.
- No thermal, spectral, atmospheric, or radiation layer is claimed in M1.
  Those are added in later milestones with their own citations.

---

## 5. Prepared-layer manifest

| Layer ID (app) | File | Kind | Resolution | Units |
|---|---|---|---|---|
| `hirise_optical` | `assets/layers/gale_hirise_ortho_1m.tif` | optical | 1.01 m/px | 8-bit reflectance DN |
| `hirise_dem` | `assets/layers/gale_hirise_dem_4m.tif` | topographic | 4 m/px | m above areoid |
| `hirise_slope` | `assets/layers/gale_hirise_slope_4m.tif` | derived topographic | 4 m/px | degrees |
| `hirise_hillshade` | `assets/layers/gale_hirise_hillshade_4m.tif` | derived topographic | 4 m/px | unitless |

M2 provides the glove-friendly layer legend: every row shows the layer name,
source, resolution and units, and toggles that raster on the map. Four layers
are available (optical, topography, slope, hillshade); they are two measured
data kinds (optical + topographic) plus two derived topographic layers, not
four independent NASA datasets. Thermal, spectral and other kinds arrive in
later milestones.

M3 adds a hazard grid, a route overlay and a field panel on top of these
layers. It does **not** add a new measured NASA data kind; the brief's
"three kinds" requirement still needs thermal, spectral or another measured
layer before the final demo. That is tracked as M4 pre-work.
