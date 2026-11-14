# M1 — load prepared Mars rasters into a zoom/pan canvas

Milestone M1 of `PLAN.md`. The data is already downloaded, processed and
documented. This brief is code only: implement raster loading and a zoom/pan
map canvas. Do not download, reprocess, or invent data.

## Non-negotiable context

- Mars only. The prepared rasters use a **Mars equirectangular** CRS:
  `+proj=eqc +lat_ts=0 +lon_0=137.39 +R=3396190 +units=m +no_defs`.
  There is no Earth/WGS84/Web-Mercator path in this app. Any layer whose CRS
  is Earth or geographic must be rejected loudly.
- The UI never touches GDAL. All GDAL access stays in `src/data/RasterLayer.cpp`.
- The four files in `assets/layers/` are real NASA HiRISE data:
  | id | file | kind | resolution | units |
  |---|---|---|---|---|
  | `hirise_optical` | `gale_hirise_ortho_1m.tif` | optical | 1.01 m/px | 8-bit reflectance DN |
  | `hirise_dem` | `gale_hirise_dem_4m.tif` | elevation | 4 m/px | m above Mars areoid |
  | `hirise_slope` | `gale_hirise_slope_4m.tif` | slope | 4 m/px | degrees |
  | `hirise_hillshade` | `gale_hirise_hillshade_4m.tif` | hillshade | 4 m/px | unitless 0–255 |
  They all share the Mars CRS above. The ortho has black no-data swath
  wedges; the DEM has nodata `-3.4028227e+38`; the slope has nodata `-9999`.
  Missing data is unknown, never traversable and never "safe".
- Site facts are fixed (from NASA product labels / HiRISE page — do not change):
  - Name: `Gale Crater — Pahrump Hills / Garden City`
  - Anchor (published observation centre): lat `-4.695`, lon `137.381`
  - Central meridian: `137.39`
  - BBox: west `137.33518211`, east `137.44270087`, south `-4.792202743651`,
    north `-4.583317928209` (degrees East / planetocentric latitude)
  - Scene size: about `6.37 km × 12.38 km`
- Do not hard-code any other coordinate or threshold. Design values must be
  named constants with a comment.

## Files you may touch

- `CMakeLists.txt`
- `src/data/SiteConfig.h`, `src/data/SiteConfig.cpp` (new)
- `src/data/RasterLayer.h`, `src/data/RasterLayer.cpp`
- `src/data/DatasetRegistry.h`, `src/data/DatasetRegistry.cpp`
- `src/gis/PlanetaryCrs.h`, `src/gis/PlanetaryCrs.cpp`
- `src/ui/MapView.h`, `src/ui/MapView.cpp`
- `src/ui/MainWindow.h`, `src/ui/MainWindow.cpp`
- `src/main.cpp` (only if needed)
- `tests/CMakeLists.txt`
- `tests/test_raster.cpp` (new)
- `tests/test_scaffold.cpp` (only to extend for the new `PlanetaryCrs` helpers)

Do **not** touch `assets/`, `data/`, `docs/`, `tools/`, `logs/`, `SidePanel`,
`analysis/`, `routing/`, or any Markdown. The data files already exist; read them
but never modify them.

## 1. `SiteConfig` — one source of truth

Create `src/data/SiteConfig.h/.cpp` with named constants for the site facts
above and for the four layer descriptions. At minimum:

```cpp
namespace SiteConfig {
inline constexpr char kSiteName[] = "Gale Crater — Pahrump Hills / Garden City";
inline constexpr double kAnchorLatDeg = -4.695;
inline constexpr double kAnchorLonDeg = 137.381;
inline constexpr double kCentralMeridianDeg = 137.39;
inline constexpr double kBboxWestLonDeg = 137.33518211;
inline constexpr double kBboxEastLonDeg = 137.44270087;
inline constexpr double kBboxSouthLatDeg = -4.792202743651;
inline constexpr double kBboxNorthLatDeg = -4.583317928209;

QString assetDir();                    // env CETUS_ASSET_DIR, else CMake default
QString layerPath(const QString &fileName);
}
```

`assetDir()` resolution order:
1. `qgetenv("CETUS_ASSET_DIR")` if non-empty;
2. the `CETUS_ASSET_DIR` compile definition (CMake sets it to
   `${CMAKE_CURRENT_SOURCE_DIR}/assets`);
3. fail loudly: return an empty string (callers turn it into a visible error),
   never silently use the current directory.

CMake:
```cmake
target_compile_definitions(cetus_core PUBLIC
    CETUS_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
```

Layer metadata strings are also constants here (or in `DatasetRegistry`, but one
place only). Use exactly these user-facing strings:

| id | title | source text | product text | resolution text | units text |
|---|---|---|---|---|---|
| `hirise_optical` | `HiRISE optical` | `HiRISE / MRO` | `ESP_040770_1755_RED_C_01_ORTHO` | `1.01 m/px` | `8-bit reflectance DN` |
| `hirise_dem` | `HiRISE elevation` | `HiRISE DTM / MRO` | `DTEEC_040770_1755_039280_1755_U01` | `4 m/px (native 1 m)` | `m above Mars areoid` |
| `hirise_slope` | `HiRISE slope` | `Derived from HiRISE DTM` | `DTEEC_040770_1755_039280_1755_U01` | `4 m/px` | `degrees` |
| `hirise_hillshade` | `HiRISE hillshade` | `Derived from HiRISE DTM` | `DTEEC_040770_1755_039280_1755_U01` | `4 m/px` | `unitless (0–255)` |

Display ranges / ramps are UI choices, not science claims:
- optical: 0–255, grayscale
- elevation: −4504 to −3473 m, dark-to-light terrain ramp
- slope: 0–40°, hazard ramp (green 0°, yellow 10°, orange 20°, red 30°+)
- hillshade: 1–255, grayscale

## 2. `PlanetaryCrs` — add the inverse

Keep the existing API and tests. Add:

```cpp
struct LonLat { double lonDeg; double latDeg; };

static LonLat eqcToLonLat(double eastingM, double northingM,
                          double centralMeridianDegrees);
static QPointF lonLatToEqc(double lonDeg, double latDeg,
                           double centralMeridianDegrees);
```

For a sphere with `lat_ts = 0`:
`x = R · (lon − lon0) · π/180`, `y = R · lat · π/180`.
Round-trip must be accurate to < 1e-9°. No Earth ellipsoid; use
`kMarsRadiusM`.

Test: `eqcToLonLat(0, 0, 137.39) == (137.39, 0)`;
`lonLatToEqc(137.381, -4.695, 137.39)` is approximately
`(-533.6 m, -278283 m)` (tolerance 1 m);
and that point lies inside the prepared scene.

## 3. `RasterLayer` — GDAL behind one module

Use a PIMPL or forward declaration so `gdal_priv.h` appears **only** in
`RasterLayer.cpp`. The header must not include GDAL headers.

```cpp
class RasterLayer
{
public:
    enum class Kind { Optical, Elevation, Slope, Hillshade };
    enum class Ramp { Grayscale, Terrain, Hazard };

    struct Metadata {
        QString id;
        QString title;
        QString source;
        QString product;
        QString resolutionText;
        QString units;
        QString fileName;
        Kind kind = Kind::Optical;
        Ramp ramp = Ramp::Grayscale;
        double displayMin = 0.0;
        double displayMax = 255.0;
    };

    struct Info {
        int width = 0;
        int height = 0;
        double pixelSizeX = 0.0;
        double pixelSizeY = 0.0;
        QRectF extent;              // projected metres, north-up
        QString crsName;
        QString crsProj4;
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

    // North-up image at exactly `targetPixels`, transparent where the raster
    // does not cover the requested extent or where the source is nodata.
    QImage readImage(const QRectF &extentMeters,
                     const QSize &targetPixels) const;
};
```

`open()` must:
1. `GDALOpenEx(..., GDAL_OF_RASTER | GDAL_OF_READONLY, ...)`; on failure,
   return false with `CPLGetLastErrorMsg()` in `errorOut`.
2. Read the projection and validate the CRS:
   - projected (not geographic),
   - semi-major axis within 1 m of `PlanetaryCrs::kMarsRadiusM`,
   - linear units ≈ 1.0 m,
   - reject EPSG:4326 / EPSG:3857 / any name containing `WGS 84` or `Earth`.
   Error text must say clearly that only a Mars projected CRS is accepted
   (include the word `Mars` and, for Earth input, `Earth`).
3. Require a north-up, non-rotated geotransform: `gt[2] == 0`, `gt[4] == 0`,
   `gt[1] > 0`, `gt[5] < 0`. Otherwise fail loudly; do not silently flip.
4. Fill `Info`: size, pixel size, extent from `gt`, band count, nodata, CRS
   name and PROJ4. Use `GDALGetProjectionRef`, `GDALGetGeoTransform`,
   `GDALGetRasterNoDataValue`, `OSRExportToProj4`.
5. Keep the dataset handle in the PIMPL, close it in the destructor.

`readImage()`:
- Clip the requested extent to `Info::extent`; if disjoint, return a fully
  transparent image of `targetPixels`.
- Compute the integer source window and the corresponding destination
  sub-rect. Use `GDALRasterIO` (or `GDALRasterIOEx`) with `GRIORA_Average` for
  Byte optical and `GRIORA_Bilinear` for float layers. Read band 1 only.
- Map Byte optical directly to gray. Map float values from
  `[displayMin, displayMax]` through the metadata ramp. Clamp values outside
  the display range; do not clamp nodata into a colour.
- Set alpha 0 for nodata (compare with a small relative tolerance for the
  huge DEM nodata) and for areas outside the raster. Never paint nodata as
  black ground.
- Orientation: source row 0 is the north edge; QImage row 0 must be north.
  Do not flip vertically. (GDAL rows and QImage rows are both top-down.)
- Thread-safety is not required; the UI calls this on the main thread.

## 4. `DatasetRegistry` — the prepared layer set

```cpp
class DatasetRegistry
{
public:
    bool loadPrepared(QString *errorOut);
    const std::vector<std::unique_ptr<RasterLayer>> &layers() const;
    const RasterLayer *layer(const QString &id) const;
    const RasterLayer *defaultLayer() const;  // hirise_optical
    QStringList ids() const;
};
```

`loadPrepared()` builds the four `RasterLayer` objects from `SiteConfig`,
opens each `SiteConfig::layerPath(fileName)`, and returns false with a message
naming the failing layer and path. Do not continue silently with a missing
layer: a missing/corrupt prepared asset is a startup error. (The window may
still open and show the error; it must not pretend the layer is there.)

Order the layers for display: hillshade, elevation, slope, optical (the
selector chooses one at a time in M1; full compositing is M2).

## 5. `MapView` — zoom/pan canvas

Keep it a `QWidget` with `QPainter` (PLAN.md §2 decision). Do not add QML.

Public API (used by `MainWindow` and tests):

```cpp
void setRegistry(const DatasetRegistry *registry);   // does not take ownership
void setActiveLayer(const QString &id);
void fitToScene();
void setViewCenter(const QPointF &eastingNorthingM);
void setMetersPerPixel(double mpp);
QPointF viewCenter() const;
double metersPerPixel() const;
QRectF visibleExtent() const;

signals:
    void cursorMoved(double lonDeg, double latDeg, double metersPerPixel);
    void viewChanged();
```

Behaviour:
- Initial view: `fitToScene()` after the registry is set. Fit the union of all
  layer extents with a small margin (5–10 %), preserving aspect ratio.
- Wheel zoom anchored under the cursor. Clamp `mpp` to
  `[0.5 × finest native pixel, fitMpp]`; use named constants/comments.
  A wheel notch should be a modest factor (about 1.25).
- Left-drag pans; use `ClosedHandCursor` while dragging and `OpenHandCursor`
  otherwise. Update the cursor readout on every mouse move.
- Draw the active layer with `RasterLayer::readImage(visibleExtent(), size())`.
  The image is already north-up and at screen resolution; draw it 1:1.
- Do **not** read the raster on every paint if the view did not change; cache
  the last image keyed by layer id, extent and widget size.
- Overlays:
  - bottom-right attribution: source, product, resolution, units, and
    `Mars eqc lon₀ 137.39° · R 3396190 m`;
  - bottom-left scale bar with a “nice” 1/2/5 × 10ⁿ length and a metre/km label;
  - top-right north arrow + `N` (orientation proof);
  - if the view contains no data, say so instead of showing a blank map.
- If the registry is null or has no active layer, keep the existing
  “no layers loaded” placeholder but say M1 / loading. If loading failed,
  `MainWindow` shows the error text; `MapView` should expose a
  `setLoadError(const QString &)` or draw from a public error string.
- Dark, high-contrast field-equipment look; no mission-control clutter.
- Handle `QWidget::size()` correctly; use `devicePixelRatioF()` only if it does
  not complicate the read path. A 1:1 logical-pixel image is acceptable for M1.

## 6. `MainWindow` / startup

- Own a `DatasetRegistry` member. Call `loadPrepared()` once. On failure,
  `qCritical()` the message, show it in the map/status bar, and keep the window
  usable (no crash, no fake layer).
- Add a compact toolbar (or a top bar inside the central widget) with:
  - a `QComboBox` listing the four layers by title, plus source/resolution in
    the item text or tooltip;
  - a `Fit` button (and optionally `+` / `−`).
  This is the M1 base-layer selector, not the full M2 legend.
- Status bar keeps `SOL 3 · 08:40 · TAU 0.6` on the left. Add a permanent
  right-side label driven by `MapView::cursorMoved`, e.g.
  `137.3810°E  4.6950°S  ·  1.01 m/px  ·  Mars eqc`.
  Also show `4 layers loaded` or the load error.
- Do not add mission logic, targets, routes, or consumables in this milestone.

## 7. CMake / tests

- Add `src/data/SiteConfig.cpp` and `src/data/SiteConfig.h` to `cetus_core`.
- Add `tests/test_raster.cpp`, register it as a `ctest` target. Link
  `cetus_core`, `Qt6::Test` and `GDAL::GDAL`.
- Set `QT_QPA_PLATFORM=offscreen` for the GUI test and
  `CETUS_ASSET_DIR=${CMAKE_SOURCE_DIR}/assets` for the real-asset test.
- Keep `tests/test_scaffold.cpp` passing; add the new `PlanetaryCrs` conversion
  tests there or in `test_raster.cpp`.
- The build must stay clean with `-Wall -Wextra`. No new warnings.

### Required tests

1. **CRS accept/reject.** Create a tiny GeoTIFF in a `QTemporaryDir` with the
   Mars CRS above and another with an Earth CRS (EPSG:3857 or EPSG:4326).
   `RasterLayer::open()` accepts Mars, rejects Earth with an error containing
   `Mars` and `Earth` (case-insensitive). Create the test rasters with the GDAL
   C++ API; no network, no external data.
2. **North-up read.** Create a 2×2 Byte raster with values
   `10 20 / 30 40` and a north-up geotransform. `readImage()` at native size
   must give `(0,0)=10`, `(1,0)=20`, `(0,1)=30`, `(1,1)=40` (grayscale red
   channel), proving no vertical flip.
3. **No-data transparency.** A float raster with `-9999` nodata: the nodata
   pixel is transparent, a valid pixel is opaque.
4. **Outside-extent transparency.** Request an extent larger than the test
   raster; the margins are transparent, the covered part is opaque.
5. **Prepared assets load.** Skip (`QSKIP`) if
   `assets/layers/gale_hirise_ortho_1m.tif` is absent. Otherwise:
   `DatasetRegistry::loadPrepared()` succeeds with 4 layers; each `isMars`,
   semi-major ≈ 3396190 m; ids match the table above; ortho pixel size ≈ 1.0118;
   ortho and DEM extents intersect; the anchor from
   `PlanetaryCrs::lonLatToEqc(137.381, -4.695, 137.39)` is inside both extents.
6. **MapView renders.** With the prepared registry, create `MapView`,
   `resize(480, 640)`, `setRegistry`, `setActiveLayer("hirise_optical")`,
   `fitToScene()`, then `grab().toImage()`. Assert the image is non-null, the
   central pixel is not the backdrop colour, and `metersPerPixel() > 0`.
   Optionally assert `visibleExtent()` contains the scene centre.
7. **Zoom/pan API.** `fitToScene()`; record `mpp`; `setMetersPerPixel(mpp/2)`;
   assert `metersPerPixel()` changed and `visibleExtent()` shrank. No mouse
   simulation required.

## 8. Verify and report

Run:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen timeout 5 ./build/cetus
```

The last command may time out (124) because the GUI event loop runs; that is
success for “no crash”. Report: files changed, test output, any warnings, and
the exact `gdalinfo`-visible metadata you used. Do not claim tests you did not
run.
