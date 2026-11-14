# Brief 06a — sample logging / field note / geotag

M6 (PLAN.md §8). On **Collect sample**, assign `SAMPLE-<sol>-<n>`, record the
target pin, Mars lon/lat and site-CRS east/north, the DEM up-vector, a field
note, and append one JSONL record to the session log. This is a field note,
not a claim that a physical sample was acquired.

## Non-negotiables

- No fabricated coordinates, minerals, instrument readings, or sample IDs.
- A missing DEM cell, missing target `phenomenon`, invalid timestamp, or empty
  log path is a **hard failure** with a loud error. Never invent a normal,
  slope, mineral, or position.
- The terrain measurement is explicitly **terrain context**, not a composition
  measurement. Record `"composition_measured": false`.
- Mars CRS only. Use `PlanetaryCrs` + `SiteConfig`; never WGS84/Web Mercator.
- Do not touch `docs/`, `assets/`, `data/`, `tools/`, `logs/`, or root `*.md`.
  The owner has already added `phenomenon` to the scenario JSON.

## Files you may change

New:

- `src/analysis/TerrainOrientation.h`, `.cpp`
- `src/app/SampleLog.h`, `.cpp`
- `src/app/SampleCollector.h`, `.cpp`
- `tests/test_samplelog.cpp`
- `tests/test_samplecollector.cpp`

Existing, narrowly:

- `src/analysis/TargetRanking.h` — add `QString phenomenon;` to
  `ScienceTarget`, after `minutesOnTarget`.
- `src/app/MissionScenario.cpp` — read optional string `phenomenon` for each
  target; if absent leave empty (not an error here; `SampleCollector` refuses
  to write a note without it).
- `src/ui/SidePanel.h`, `.cpp` — add the Collect button + latest-sample /
  error labels.
- `src/ui/MainWindow.h`, `.cpp` — own the collector, wire the button, expose
  test accessors, add sample failure to `updateStatus()`.
- `tests/test_ui.cpp` — add one end-to-end sample test.
- `CMakeLists.txt`, `tests/CMakeLists.txt` — register sources/tests.

Do not restructure M3 routing/ranking, do not touch `MapView`, do not add M4/M5
walkback/consumables/scenario-state code.

## 1. `TerrainOrientation`

```cpp
struct SurfaceOrientation
{
    double upEast = 0.0, upNorth = 0.0, upUp = 1.0; // unit normal, ENU
    double slopeDeg = 0.0;                           // 0..90
    double aspectDeg = 0.0;                          // downslope, clockwise from N
    bool aspectDefined = false;                      // false on flat ground
    QString method;                                  // human-readable
};

class TerrainOrientation
{
public:
    static bool fromDem(const RasterLayer &dem, const QPointF &eastingNorthing,
                        SurfaceOrientation *out,
                        QString *errorOut = nullptr);
};
```

Rules:

- Require `dem.metadata().kind == RasterLayer::Kind::Elevation`; otherwise
  fail with an error containing `"elevation"`.
- Locate the target's cell from `dem.info()` and read a **3×3** neighbourhood
  with `RasterLayer::readValues` using the exact source-pixel rectangle.
  Row 0 is north (the raster API is north-up).
- Any NaN in the 9 samples is a hard failure: `"DEM has no data at the sample
  position; refusing to invent an orientation."`.
- Horn 3×3 gradient (same convention as `gdaldem`):
  `dzdx` positive east, `dzdy` positive north, denominator `8 * pixelSize`.
- Normal `(-dzdx, -dzdy, 1)` normalised to unit length. Do **not** use an
  average plane or a one-sided difference.
- `slopeDeg = atan(hypot(dzdx, dzdy))` in degrees.
- `aspectDeg = atan2(-dzdx, -dzdy)` in degrees clockwise from north,
  normalised to `[0, 360)`. Set `aspectDefined=false` when
  `hypot(dzdx,dzdy) < 1e-6` (do not report a fake aspect on flat ground).
- `method = "Horn 3x3 surface gradient on <px> m/px DEM"` using the actual
  `info.pixelSizeX`.
- Unit-test it with the real DEM at Pahrump Hills; slope must be within 1.0°
  of 2.90° (the corrected-product sample in `docs/data-sources.md`), and the
  normal length within `1e-6` of 1.

## 2. `SampleLog`

```cpp
struct SampleRecord
{
    QString id;                    // SAMPLE-<sol>-<n>
    int sol = -1;
    QDateTime timestampUtc;

    QString targetId, targetName, targetSource;
    double eastingM = 0.0, northingM = 0.0;
    double lonDeg = 0.0, latDeg = 0.0;

    double upEast = 0.0, upNorth = 0.0, upUp = 1.0;
    double slopeDeg = 0.0, aspectDeg = 0.0;
    bool aspectDefined = false;
    QString orientationMethod, orientationSource;

    QString phenomenon, why, measurement;
    QString positionSource;
    bool compositionMeasured = false;
};

class SampleLog
{
public:
    explicit SampleLog(QString filePath);

    bool load(QString *errorOut = nullptr);       // missing file == empty, success
    bool append(SampleRecord *record, QString *errorOut = nullptr);

    QString nextId(int sol) const;                // SAMPLE-<sol>-<next n>
    const QVector<SampleRecord> &records() const;
    QString filePath() const;
    int sampleCount() const;

    static QString defaultPath();                 // env override + app data dir
    static QJsonObject toJson(const SampleRecord &record);
    static bool fromJson(const QJsonObject &object, SampleRecord *record,
                         QString *errorOut = nullptr);
};
```

- `defaultPath()`: non-empty `CETUS_SAMPLE_LOG` wins; else
  `QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
  "/samples.jsonl"`. Return empty if no writable location; caller fails loudly.
- `nextId(sol)`: parse existing `SAMPLE-<sol>-<n>` IDs, return max `n + 1`
  (start at 1). Count only that sol, not total records.
- `append` assigns `record->id` when empty. Validate first; create parent
  directories; open in append mode; write one compact JSON object + `\n`;
  `flush()` and check errors. Do not rewrite or truncate the file.
- `load`: missing file is okay/empty. Otherwise read JSONL, skip blank lines,
  parse every line strictly. A malformed line, duplicate ID, or bad schema
  fails with the file path and line number. Never silently discard a line.
- `toJson` shape (use these keys):

```json
{
  "schema": "cetus.sample/1",
  "id": "SAMPLE-3-1",
  "sol": 3,
  "timestamp_utc": "2026-11-14T22:15:00.000Z",
  "target": {"id": "T1", "name": "Pahrump Hills", "source": "..."},
  "position": {
    "easting_m": -756.135, "northing_m": -276363.057,
    "lon_deg": 137.37724, "lat_deg": -4.662412,
    "crs": {
      "name": "Mars equirectangular, planetocentric",
      "proj4": "+proj=eqc +lat_ts=0 +lon_0=137.39 +R=3396190 +units=m +no_defs",
      "radius_m": 3396190.0, "central_meridian_deg": 137.39
    }
  },
  "orientation": {
    "up_east": 0.0, "up_north": 0.0, "up_up": 1.0,
    "slope_deg": 2.9, "aspect_deg": 0.0, "aspect_defined": false,
    "method": "Horn 3x3 surface gradient on 4 m/px DEM",
    "source": "HiRISE DTM / MRO · <product>"
  },
  "field_note": {
    "phenomenon": "...", "why": "...",
    "measurement": "Terrain context ... no in-situ composition measurement recorded.",
    "composition_measured": false
  },
  "provenance": {
    "recorded_by": "Cetus field tool (operator-entered field note)",
    "position_source": "NASA Mars Trek target pin transformed to the site Mars equirectangular CRS",
    "orientation_source": "Derived from HiRISE DTM at 4 m/px",
    "note": "Field note only; no physical sample or composition measurement is claimed."
  }
}
```

- Validate: non-empty id/target fields/phenomenon/why, finite coordinates,
  `lon` in `[-180,180]`, `lat` in `[-90,90]`, unit normal (`|len-1| <= 1e-3`),
  `slope` in `[0,90]`, aspect in `[0,360)` when defined, valid timestamp.
- `fromJson` must reject missing required keys instead of defaulting silently.

## 3. `SampleCollector`

```cpp
class SampleCollector
{
public:
    SampleCollector(const RasterLayer *dem, QString logPath);

    bool load(QString *errorOut = nullptr);  // validates DEM + existing log
    bool isReady() const;

    bool collect(const ScienceTarget &target, int sol,
                 const QDateTime &timestampUtc,
                 SampleRecord *out, QString *errorOut = nullptr);

    const SampleLog &log() const;
    QString logPath() const;

private:
    const RasterLayer *m_dem = nullptr;
    SampleLog m_log;
    bool m_loaded = false;
};
```

- `load` requires a non-null, valid elevation DEM and a non-empty log path;
  otherwise fail containing `"DEM"` / `"log"`.
- `collect`:
  - reject invalid timestamp/sol, or `target.phenomenon.trimmed().isEmpty()`
    with an error naming the target and `phenomenon` (never fall back to the
    target name or "mineral vein");
  - `TerrainOrientation::fromDem` at the target pin;
  - `PlanetaryCrs::eqcToLonLat(target.eastingM, target.northingM,
    SiteConfig::kCentralMeridianDeg)`;
  - set `positionSource`, `orientationSource` from the DEM metadata
    (`product`, `source`, actual pixel size);
  - `measurement` must be a single honest sentence containing the slope and
    either the aspect or `"aspect not defined (locally flat)"`, and ending
    `"No in-situ composition measurement was recorded."`;
  - `compositionMeasured = false`;
  - append via `SampleLog`; return the stored record.
- Do not create a sample if any step fails.

## 4. UI

`SidePanel` additions (keep all existing wording/order; add after the primary
target's `why`, before `CONSUMABLES`):

- `QPushButton` object name `collectSampleButton`, min height 44, amber,
  dark bold text, tooltip `"Appends a field note to the session log; no
  physical sample is claimed."`.
- `QLabel` object name `latestSampleLabel`; starts `"No samples logged."`.
- `QLabel` object name `sampleErrorLabel`; starts hidden.
- Public API:

```cpp
void setCollectTarget(const QString &targetId); // empty => disabled
void setLatestSample(const QString &sampleId, const QString &summary, int total);
void setSampleLogError(const QString &error);   // empty => hide
QString collectTargetId() const;
QString latestSampleId() const;
QString sampleErrorText() const;
signals: void collectSampleRequested();
```

Button text: `COLLECT SAMPLE` when disabled/empty, `COLLECT SAMPLE AT T1` when
enabled. `setLatestSample` shows the ID, summary, and `"<n> logged"`; do not
hide older info. `setSampleLogError` must be visually loud (red border/background,
like the warning label) but must not block the map.

`MainWindow`:

- Own `std::unique_ptr<SampleCollector>`; declare an out-of-line destructor
  (incomplete type).
- After a successful `loadMission()` (scenario + hazard + route), call
  `openSampleLog()`:
  - DEM = `registry.layer("hirise_dem")`, path = `SampleLog::defaultPath()`;
  - on failure store `m_sampleError`, `qCritical()`, show it in the panel and
    status; leave the map usable;
  - on success `m_sidePanel->setCollectTarget(m_primaryTargetId)`.
- Connect `SidePanel::collectSampleRequested` to `MainWindow::collectSample()`
  once in the constructor.
- `collectSample()` uses the **primary target** and sol `3` (named
  `[demo]` constant; M4 owns live mission state), calls
  `QDateTime::currentDateTimeUtc()`, then `setLatestSample`. On failure store
  `m_sampleError`, `qCritical()`, panel error, `updateStatus()`; on success
  clear the error and `updateStatus()`.
- `updateStatus()` priority: `RASTER LOAD FAILED` > `MISSION DATA FAILED` >
  `SAMPLE LOG FAILED` > normal. Normal appends
  `" · SAMPLE <latest id>"` when a sample exists.
- Test accessors: `const SampleCollector *sampleCollector() const;`,
  `int sampleCount() const;`, `QString sampleLogPath() const;`.

## 5. Tests (must actually run)

`tests/test_samplelog.cpp`:

- `appendsJsonlAndContinuesPerSol()`: temp file, append two sol-3 records and
  one sol-4; IDs `SAMPLE-3-1`, `SAMPLE-3-2`, `SAMPLE-4-1`; file has 3 lines;
  load back into a new `SampleLog` and assert all fields/provenance and that
  `nextId(3) == SAMPLE-3-3`.
- `rejectsInvalidRecord()`: missing phenomenon, NaN easting, non-unit normal,
  duplicate ID each return false and write nothing.
- `corruptLogFailsLoudly()`: write `not json`, `load()` false with line/path.
- `defaultPathHonoursEnvironment()`: set `CETUS_SAMPLE_LOG`, compare; restore.

`tests/test_samplecollector.cpp`:

- real assets guard via `CETUS_ASSET_DIR` + `gale_hirise_dem_4m.tif`; skip if
  absent;
- load scenario T1, collect at a fixed UTC time, assert:
  - id, target id/name, easting/northing equal the scenario pin;
  - lon/lat within `1e-6` of the scenario lon/lat;
  - normal length within `1e-6` of 1, slope within 1.0° of 2.90°;
  - `phenomenon` contains `Jarosite`, `compositionMeasured == false`,
    measurement contains `No in-situ composition measurement`;
  - the JSONL file exists and contains one parseable object.
- `missingDemFailsLoudly()`: null DEM → load false and error contains `DEM`.
- `missingPhenomenonFailsLoudly()`: copy T1, clear phenomenon → collect false,
  file unchanged.

`tests/test_ui.cpp` new `collectsSampleIntoSessionLog()`:

- temp `CETUS_SAMPLE_LOG`; construct `MainWindow`; assert `scenarioLoaded()`;
- find `collectSampleButton`; enabled and text contains `T1`;
- click; `sampleCount() == 1`; panel `latestSampleId() == SAMPLE-3-1`;
  status contains `SAMPLE-3-1`; log has one line;
- construct a second `MainWindow` with the same env; click again; ID is
  `SAMPLE-3-2`; log has two lines; restore env afterwards.
- Do not rely on test order; use a unique `QTemporaryDir` inside the test.

CMake: add `TerrainOrientation`, `SampleLog`, `SampleCollector` to
`cetus_core`; add `test_samplelog` and `test_samplecollector` with
`QT_QPA_PLATFORM=offscreen;CETUS_ASSET_DIR=...`.

## Verification

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Report exact totals. No visual-check claim.
