# M1a — raster core (no GUI)

Implement the data side of M1. The prepared NASA rasters already exist in
`assets/layers/`; do not download, reprocess or modify them. Do not explore the
data or the repository: every constant and path you need is in this brief.
Start writing code immediately. Time budget: keep it under 30 minutes.

## Allowed files
- `CMakeLists.txt`
- `src/data/SiteConfig.h`, `src/data/SiteConfig.cpp` (new)
- `src/data/RasterLayer.h`, `src/data/RasterLayer.cpp`
- `src/data/DatasetRegistry.h`, `src/data/DatasetRegistry.cpp`
- `src/gis/PlanetaryCrs.h`, `src/gis/PlanetaryCrs.cpp`
- `tests/CMakeLists.txt`
- `tests/test_raster.cpp` (new)
- `tests/test_scaffold.cpp` (only to add the new PlanetaryCrs tests)

Do not touch `src/ui/`, `src/app/`, `src/analysis/`, `src/routing/`, `assets/`,
`data/`, `docs/`, `tools/`, `logs/`, or Markdown.

## Fixed facts (do not change)
- Mars radius: `3396190.0 m`.
- Site CRS: `+proj=eqc +lat_ts=0 +lon_0=137.39 +R=3396190 +units=m +no_defs`.
- Site name: `Gale Crater — Pahrump Hills / Garden City`.
- Anchor: lat `-4.695`, lon `137.381`.
- BBox: west `137.33518211`, east `137.44270087`, south `-4.792202743651`,
  north `-4.583317928209`.
- Four layers, all in that CRS:
  | id | file | kind | resolution text | units text | display range | ramp |
  |---|---|---|---|---|---|---|
  | `hirise_optical` | `gale_hirise_ortho_1m.tif` | optical | `1.01 m/px` | `8-bit reflectance DN` | 0..255 | grayscale |
  | `hirise_dem` | `gale_hirise_dem_4m.tif` | elevation | `4 m/px (native 1 m)` | `m above Mars areoid` | -4504..-3473 | terrain |
  | `hirise_slope` | `gale_hirise_slope_4m.tif` | slope | `4 m/px` | `degrees` | 0..40 | hazard |
  | `hirise_hillshade` | `gale_hirise_hillshade_4m.tif` | hillshade | `4 m/px` | `unitless (0–255)` | 1..255 | grayscale |
  Source strings: optical `HiRISE / MRO`, dem/slope/hillshade `HiRISE DTM / MRO`
  (derived layers may use `Derived from HiRISE DTM`). Product strings:
  `ESP_040770_1755_RED_C_01_ORTHO`, `DTEEC_040770_1755_039280_1755_U01`,
  `DTEEC_040770_1755_039280_1755_U01`, `DTEEC_040770_1755_039280_1755_U01`.
- Nodata: DEM `-3.4028227e+38`; slope `-9999`; hillshade `0`; optical has no
  nodata flag (black swath edges are DN 0 and may render black).

## 1. `SiteConfig` (new)
Header + cpp. Put the constants above in `namespace SiteConfig`.
```cpp
QString assetDir();                 // CETUS_ASSET_DIR env, else compile macro
QString layerPath(const QString &fileName);
```
`assetDir()` order: non-empty `qgetenv("CETUS_ASSET_DIR")`; else the macro
`CETUS_ASSET_DIR`; else return an empty string (callers must fail loudly).
Do not fall back to the current directory.

CMake: add `src/data/SiteConfig.cpp`/`.h` to `cetus_core`, and
`target_compile_definitions(cetus_core PUBLIC CETUS_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")`.

Also expose the four `RasterLayer::Metadata` descriptors as functions or
constants in `SiteConfig` so the registry and tests use one source. Keep the
user-facing strings exactly as above.

## 2. `PlanetaryCrs` additions
Keep the current API and tests. Add:
```cpp
struct LonLat { double lonDeg; double latDeg; };
static LonLat eqcToLonLat(double eastingM, double northingM,
                          double centralMeridianDegrees);
static QPointF lonLatToEqc(double lonDeg, double latDeg,
                           double centralMeridianDegrees);
```
Sphere, `lat_ts = 0`:
`x = R*(lon-lon0)*pi/180`, `y = R*lat*pi/180`.
Tests in `test_scaffold.cpp`:
- `eqcToLonLat(0,0,137.39) == (137.39, 0)` within `1e-9`.
- `lonLatToEqc(137.381,-4.695,137.39)` round-trips within `1e-9`.
- That projected point is inside the site bbox after converting the bbox
  corners with `lonLatToEqc` (compare x/y ranges). Do not hard-code a y value.

## 3. `RasterLayer`
Header must NOT include GDAL. Use PIMPL (`struct Impl; std::unique_ptr<Impl>`)
and declare the destructor/move operations out of line. Exact public API:
```cpp
class RasterLayer {
public:
  enum class Kind { Optical, Elevation, Slope, Hillshade };
  enum class Ramp { Grayscale, Terrain, Hazard };
  struct Metadata {
    QString id, title, source, product, resolutionText, units, fileName;
    Kind kind = Kind::Optical;
    Ramp ramp = Ramp::Grayscale;
    double displayMin = 0.0, displayMax = 255.0;
  };
  struct Info {
    int width = 0, height = 0;
    double pixelSizeX = 0.0, pixelSizeY = 0.0;
    QRectF extent;              // projected metres, north-up
    QString crsName, crsProj4;
    double semiMajorAxisM = 0.0;
    bool isMars = false;
    int bandCount = 0;
    bool hasNoData = false;
    double noDataValue = 0.0;
  };
  explicit RasterLayer(Metadata metadata);
  ~RasterLayer();
  RasterLayer(const RasterLayer &) = delete;
  RasterLayer &operator=(const RasterLayer &) = delete;
  RasterLayer(RasterLayer &&) noexcept;
  RasterLayer &operator=(RasterLayer &&) noexcept;
  bool open(const QString &path, QString *errorOut);
  bool isValid() const;
  const Metadata &metadata() const;
  const Info &info() const;
  QImage readImage(const QRectF &extentMeters,
                   const QSize &targetPixels) const;
};
```
`open()`:
- `GDALOpenEx(path, GDAL_OF_RASTER|GDAL_OF_READONLY, ...)`.
- Validate CRS: projected, `|GetSemiMajor()-3396190| < 1`, linear units ~1 m,
  and reject geographic, EPSG:4326, EPSG:3857, names containing `WGS 84` or
  `Earth`. Error text must contain `Mars` and, for Earth input, `Earth`.
- Require north-up: `gt[2]==0`, `gt[4]==0`, `gt[1]>0`, `gt[5]<0`; otherwise
  fail with a clear error (do not flip silently).
- Fill `Info` with size, pixel size, extent (`gt[0]`, `gt[3]`, width/height),
  bands, nodata, CRS name and PROJ4.
- Close the GDAL handle in the destructor.

`readImage()`:
- Return `QImage(targetPixels, QImage::Format_ARGB32)` filled transparent.
- Clip to `Info::extent`; disjoint -> all transparent.
- Compute integer source window and destination sub-rect; use
  `GDALRasterIO` with `GRIORA_Average` for Byte and `GRIORA_Bilinear` for float.
- Byte optical -> grayscale (value -> R=G=B=value, A=255).
- Float -> stretch `displayMin..displayMax` through the ramp:
  - Terrain: dark blue/black -> brown -> tan -> white.
  - Hazard: green -> yellow -> orange -> red.
  - Grayscale: black -> white.
  Clamp values outside the display range, but never map nodata to a colour.
- Nodata: alpha 0 (use a relative tolerance for the huge DEM value). Areas
  outside the raster: alpha 0.
- Rows stay top-down: QImage row 0 is the north edge. Do not flip.

## 4. `DatasetRegistry`
```cpp
class DatasetRegistry {
public:
  bool loadPrepared(QString *errorOut);
  const std::vector<std::unique_ptr<RasterLayer>> &layers() const;
  const RasterLayer *layer(const QString &id) const;
  const RasterLayer *defaultLayer() const;  // hirise_optical
  QStringList ids() const;
};
```
`loadPrepared()` builds the four descriptors from `SiteConfig`, opens
`SiteConfig::layerPath(fileName)`, and returns false with the failing id/path
on any error. Display order: hillshade, dem, slope, optical.
No silent skip of a missing layer.

## 5. CMake / tests
- Add the new sources to `cetus_core`.
- Add `tests/test_raster.cpp`; link `cetus_core` and `Qt6::Test`.
  Add `GDAL::GDAL` only if the test uses the GDAL API (it does).
- Register a `ctest` test named `raster`.
- Set env for the test: `CETUS_ASSET_DIR=${CMAKE_SOURCE_DIR}/assets`.
- Keep `-Wall -Wextra` clean.

Tests in `test_raster.cpp` (`QTEST_GUILESS_MAIN`):
1. Create a 2x2 Byte GeoTIFF in `QTemporaryDir` with the Mars CRS and a
   north-up gt `[0,1,0,2,0,-1]`, values `10 20 / 30 40`. `open()` succeeds;
   `isMars`; semi-major ~3396190. `readImage(QRectF(0,0,2,2), QSize(2,2))`
   gives red channel `10,20,30,40` at `(0,0),(1,0),(0,1),(1,1)`. This proves
   north-up.
2. Same but EPSG:3857 (or 4326): `open()` fails and the error contains `Mars`
   and `Earth` (case-insensitive).
3. Float raster with nodata `-9999`: nodata pixel alpha 0, valid pixel alpha
   255.
4. Request an extent larger than the raster: margins alpha 0, covered centre
   alpha 255.
5. Real assets: `QSKIP` if `assets/layers/gale_hirise_ortho_1m.tif` is absent.
   Else `DatasetRegistry::loadPrepared()` succeeds with 4 layers; every
   `isMars` and semi-major ~3396190; ids match; ortho pixel size ~1.0118;
   ortho/DEM extents intersect; `lonLatToEqc(137.381,-4.695,137.39)` is inside
   both extents.
6. A rotated geotransform is rejected (optional but cheap).

Create test rasters with the GDAL C++ API (`GDALCreate`, `SetGeoTransform`,
`SetProjection`, `RasterIO`). No network.

## 6. Verify
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```
The existing `scaffold` test must still pass. Report files changed, commands,
test output, and warnings. Do not claim anything you did not run.
