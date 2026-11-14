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
- **Do not guess coordinates for individual vein targets.** The T1/T2/T3 pins
  will be digitised from the 1 m HiRISE ortho in M3 and cross-checked against
  the NASA browse images. Until then, only the site footprint and the published
  observation centre are used.
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
topography share a grid and a CRS. “North is up” per the HiRISE product page.
The JP2 has black triangular no-data wedges at the swath edges; they are *not*
terrain and must never be treated as traversable data.

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
| Slope | `assets/layers/gale_hirise_slope_4m.tif` | degrees | `gdaldem slope -p -alg Horn` on the 4 m DEM |
| Hillshade | `assets/layers/gale_hirise_hillshade_4m.tif` | unitless 0–255 | `gdaldem hillshade -z 2 -az 315 -alt 45` on the 4 m DEM |

The hillshade illumination (azimuth 315°, altitude 45°, z-factor 2) is a
**display choice**, not a measured solar position. The HiRISE page gives a
solar incidence angle of 33° for the source image; if a physically consistent
shade is needed later, use the acquisition geometry. Slope is computed at 4 m
post spacing, so it smooths metre-scale boulders; boulder-scale hazard will
need the 1 m DTM or optical texture in M3.

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
gdaldem slope -p -alg Horn assets/layers/gale_hirise_dem_4m.tif /tmp/slope4m.tif
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

The app displays source, resolution and units for every layer (M2 formalises
the legend; M1 shows the active layer's provenance in the status bar).
