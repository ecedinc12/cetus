# M3a — numeric raster grid, hazard map, target ranking, scenario loader

Implement the analysis foundation for M3. This is **code only**; the scenario
JSON already exists at `assets/scenario/m3_scenario.json` and must be read,
not edited.

## Allowed files
- `src/data/RasterLayer.h`, `src/data/RasterLayer.cpp`
- `src/data/SiteConfig.h`, `src/data/SiteConfig.cpp`
- `src/analysis/HazardMap.h`, `src/analysis/HazardMap.cpp`
- `src/analysis/TargetRanking.h`, `src/analysis/TargetRanking.cpp`
- `src/app/MissionScenario.h`, `src/app/MissionScenario.cpp` (new)
- `CMakeLists.txt` (add the new .h/.cpp files)
- `tests/test_analysis.cpp` (new)
- `tests/CMakeLists.txt` (add the test target)

Do not touch `src/routing/`, `src/ui/`, `assets/`, `data/`, `docs/`, or any
other file. Do not change the existing four raster layers or their metadata.
Do not add synthetic raster data. Do not use Earth/WGS84/WebMercator.

## 1. `RasterLayer::readValues`

The UI only has `readImage()`. Analysis needs numeric samples without GDAL
leaking out of `RasterLayer` (AGENTS.md §6). Add exactly this public method:

```cpp
// Row-major numeric samples, row 0 = north edge (same orientation as
// readImage). NaN means nodata/outside the raster. Returns an empty vector
// and sets *ok=false for invalid arguments or a GDAL read failure.
QVector<float> readValues(const QRectF &extentMeters, int width, int height,
                          bool *ok = nullptr) const;
```

Rules:
- Include `<QVector>` in the header.
- Reuse the existing `Info::extent` convention: `x`/`left` = west,
  `y`/`top` = south, `right` = east, `bottom` = north. Row 0 is north.
- Use band 1, `GDT_Float32`, `GRIORA_NearestNeighbour` (nearest-neighbour is
  deliberate: a 20° no-go boundary must not be averaged into a safe value).
- Fill pixels outside the raster and nodata with `std::numeric_limits<float>::quiet_NaN()`.
- Reuse the existing `isNoData()` tolerance for nodata (DEM nodata is huge;
  slope nodata is `-9999`).
- If the requested extent is disjoint from the raster, return a
  width*height NaN vector with `*ok=true`.
- `*ok=true` only when a valid output vector of exactly `width*height` was
  produced. Never silently return a short vector.
- Do not change `readImage()` behaviour.

## 2. `HazardMap`

Replace the placeholder with a real slope-hazard grid. Header must expose:

```cpp
class RasterLayer;

class HazardMap
{
public:
    // [tunable] PLAN.md §5.3: slope above this is a hard no-go.
    static constexpr double kNoGoSlopeDeg = 20.0;
    // [tunable] route grid resolution.
    static constexpr double kCellSizeM = 10.0;

    struct Stats {
        int cellCount = 0;
        int noGoCount = 0;
        int unknownCount = 0;
        double maxSlopeDeg = 0.0;
    };

    bool build(const RasterLayer &slopeLayer, double cellSizeM,
               QString *errorOut = nullptr);
    bool isValid() const;
    QRectF extentMeters() const;    // same convention as RasterLayer::Info::extent
    double cellSizeM() const;
    int width() const;
    int height() const;
    bool contains(double eastM, double northM) const;
    double slopeDeg(int col, int row) const;      // NaN for unknown/invalid
    bool isNoGo(int col, int row) const;          // unknown/outside => true
    bool isNoGoAt(double eastM, double northM) const;
    double cost(int col, int row) const;          // infinity for no-go
    QPointF cellCenter(int col, int row) const;
    QImage hazardImage(const QSize &targetPixels) const;
    Stats stats() const;
    QString sourceDescription() const;
};
```

Implementation notes:
- Build the grid over the **full extent of the slope layer**; never shrink it
  to the target/route. `width = ceil(extent.width()/cellSizeM)`, same for height.
- Sample cell centres with `RasterLayer::readValues(extent, width, height)`.
- Nodata/NaN and outside are **unknown** and `isNoGo() == true` (AGENTS.md
  §4: missing means unknown, never safe).
- A finite slope `> kNoGoSlopeDeg` is no-go. Exactly 20° is traversable.
- Traversable cost multiplier: `1.0 + 2.0 * (slope/20.0)^2` (range 1..3).
- `hazardImage` is a diagnostic overlay: no-go/unknown = red with alpha ~170,
  slope >= 10° but < 20° = amber with alpha ~120, otherwise transparent.
  Row 0 must be north.
- `sourceDescription()` returns e.g.
  `"HiRISE slope · 4 m/px · degrees · no-go > 20°"` — use the slope layer's
  metadata, not a hard-coded source if it is available.
- Fail loudly: empty error string on success; on failure return false and put a
  specific message in `errorOut` (invalid layer, bad cell size, empty grid,
  read failure).

## 3. `TargetRanking`

Replace the placeholder. Define the shared model here so the routing and UI
milestones can include this header:

```cpp
struct ScienceTarget {
    QString id;             // "T1" etc.
    QString name;
    QString why;
    QString source;
    double eastingM = 0.0;
    double northingM = 0.0;
    double scienceValue = 0.0;    // 0..1, scenario design choice
    double minutesOnTarget = 0.0;
    QPointF position() const { return QPointF(eastingM, northingM); }
};

struct RankedTarget {
    ScienceTarget target;
    double travelMinutes = 0.0;
    double valueRate = 0.0;
    bool reachable = false;
};

class TargetRanking
{
public:
    static double valueRate(double scienceValue, double travelMinutes,
                            double minutesOnTarget);
    static QVector<RankedTarget> rank(
        const QVector<ScienceTarget> &targets,
        const QHash<QString, double> &travelMinutesById);
};
```

Rules:
- `valueRate = scienceValue / (travelMinutes + minutesOnTarget)` exactly
  (PLAN.md §5.2). Guard non-positive denominator by returning 0.
- Missing/negative travel time => `reachable=false`, `travelMinutes=0`,
  `valueRate=0`.
- Sort reachable targets first by `valueRate` descending, then by
  `travelMinutes` ascending, then by `id`. Unreachable targets after them,
  sorted by `id`. Never drop a target.
- Do not add hazard/walkback logic here; that belongs to the planner.

## 4. `MissionScenario` + default path

New `src/app/MissionScenario.h`:

```cpp
struct MissionScenario
{
    QString source;
    QString crewName;
    QString crewSource;
    QPointF crewStart;
    QVector<ScienceTarget> targets;

    static bool loadFromFile(const QString &path, MissionScenario *out,
                             QString *errorOut = nullptr);
};
```

Add to `SiteConfig`:

```cpp
// <assetDir>/scenario/m3_scenario.json
QString scenarioPath();
```

Loader rules:
- Read with `QFile` + `QJsonDocument` (Qt Core; no extra dependency).
- Require `schema == "cetus.m3.scenario/1"`.
- Require `crs.radius_m` within 1.0 m of `SiteConfig::kMarsRadiusM` and
  `crs.central_meridian_deg` within 1e-6 of `SiteConfig::kCentralMeridianDeg`;
  otherwise fail with a message mentioning the Mars CRS.
- Read `crew` (`name`, `source`, `easting_m`, `northing_m`) and every target
  (`id`, `name`, `why`, `source`, `easting_m`, `northing_m`,
  `science_value`, `minutes_on_target`). Reject missing fields, duplicate IDs,
  non-finite coordinates, `science_value` outside 0..1, or
  `minutes_on_target <= 0`.
- `out` is only modified on success. Error text includes the path.
- Do not hard-code the three target names in the loader; the JSON is the data.

## 5. Tests (`tests/test_analysis.cpp`)

Use `QT_QPA_PLATFORM=offscreen` and `CETUS_ASSET_DIR=${CMAKE_SOURCE_DIR}/assets`.
Add the test target in `tests/CMakeLists.txt` with that environment.

Test slots:
1. `readsSlopeValues()` — open `hirise_slope`; read the full extent at 64×64;
   `ok`, size 4096; all finite values are 0..90; at least one finite and at
   least one NaN; a request partially outside the raster has NaN in the outside
   strip; `width=0` returns empty and `ok=false`.
2. `buildsHazardMap()` — build at 20 m; valid; extent equals slope extent
   (within 1e-6); stats.cellCount == width*height; noGoCount > 0;
   unknownCount > 0; `maxSlopeDeg >= 20`; `sourceDescription()` contains
   `"degrees"` and `"20"`.
3. `mariasPassIsNoGo()` — `isNoGoAt(-1102.35727505766, -276690.662501919)`
   is true (real 4 m slope ~24°); `isNoGoAt(-756.135485790287,
   -276363.057354002)` (Pahrump) is false; cost is infinity at the first and
   finite at the second. This test documents a real hazard, not a fabricated
   one.
4. `hazardImageRowsAreNorthUp()` — `hazardImage(QSize(320, 640))` has size;
   at least one pixel has alpha > 0; not all pixels transparent.
5. `ranksByValueRate()` — three synthetic targets and travel times; verify
   exact formula, descending order, unreachable last, duplicate rank stability.
6. `loadsRealScenario()` — `MissionScenario::loadFromFile(
   SiteConfig::scenarioPath(), &s, &err)`; 3 targets; IDs T1/T2/T3; crew start
   near (-753.397,-276224.802); T2 has `scienceValue == 0.95`; `why` mentions
   `"fractures"`; `source` mentions `"Mars Trek"`.
7. `rejectsBadScenario()` — write a temp JSON with radius 6371000 (Earth) and
   verify failure text mentions Mars; write duplicate IDs and verify failure.
   Use `QTemporaryDir`.

## Verify
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```
All existing tests plus `analysis` must pass. Report the commands run and any
test that was skipped and why. Do not claim a test passed if it was not run.
