# M1b — MapView canvas (one file pair)

Implement only the map canvas. The raster core (`RasterLayer`,
`DatasetRegistry`, `SiteConfig`) is finished and tested. Do not change it.
Do not explore the repo: read only `src/ui/MapView.h`, `src/ui/MapView.cpp`,
`src/data/RasterLayer.h`, `src/data/DatasetRegistry.h`. Start writing now.

## Allowed files
- `src/ui/MapView.h`
- `src/ui/MapView.cpp`
- `tests/test_mapview.cpp` (new)
- `tests/CMakeLists.txt`

Do not touch `MainWindow`, `SidePanel`, `src/data/`, `src/gis/`, `src/app/`,
`CMakeLists.txt`, `assets/`, `data/`, `docs/`, `tools/`, `logs/`, or Markdown.

## API (use exactly this)
```cpp
class MapView : public QWidget {
    Q_OBJECT
public:
    explicit MapView(QWidget *parent = nullptr);

    // Non-owning. Passing nullptr clears the view.
    void setRegistry(const DatasetRegistry *registry);
    void setActiveLayer(const QString &layerId);
    QString activeLayerId() const;

    void fitToScene();
    void setMetersPerPixel(double metersPerPixel);
    double metersPerPixel() const;

    // x = west, y = south, width/height = metres. y grows north, so
    // top() is the south edge and bottom() is the north edge.
    QRectF visibleExtentMeters() const;

public slots:
    void zoomIn();
    void zoomOut();

signals:
    void cursorMoved(double eastingM, double northingM);
    void viewChanged();
};
```
`Q_OBJECT`, default constructor parent argument unchanged.

## Behaviour
- Keep a view centre `QPointF m_center` in projected metres and
  `double m_mpp`. Defaults: mpp `1.0`, centre `(0,0)`.
- `setRegistry`: store pointer; if a default layer exists, set the active id
  and `fitToScene()`.
- `setActiveLayer`: ignore unknown ids; otherwise store, `fitToScene()`, update.
- `fitToScene`: union the extents of all registry layers (`Info::extent`) or of
  the active layer only; centre on the union; set mpp so the union fits the
  widget with 5% margin on each side (use `qMin` of x/y fit). Clamp mpp to
  `[0.25, 200.0]`. Emit `viewChanged()`.
- `setMetersPerPixel`: clamp, store, emit `viewChanged()`.
- `zoomIn`/`zoomOut`: multiply/divide mpp by `1.25`, clamp, keep centre.
- `visibleExtentMeters`: `w = width()*mpp`, `h = height()*mpp`,
  `QRectF(m_center.x()-w/2, m_center.y()-h/2, w, h)`.
- `paintEvent`:
  - fill the widget with `QColor(24, 18, 15)`.
  - if no registry/active layer: draw centred text
    `MAP VIEW — no layers loaded` and return.
  - `QImage img = layer->readImage(visibleExtentMeters(), size());`
    draw it at `(0,0)`. `readImage` is already north-up; **never flip it**.
  - overlay a north arrow at the top-right (line + `N`) and a scale bar at
    the bottom-left whose label is a round number of metres computed from
    `m_mpp * 120` (e.g. 100 m, 250 m) drawn with `QPainter`.
  - overlay one attribution line at the bottom:
    `"<source> · <resolutionText> · <units>"` from `layer->metadata()`.
  - use high-contrast pen/brush colours (light text, dark translucent bar).
- Mouse:
  - left press: start pan, store position, set cursor `ClosedHandCursor`.
  - move: if panning, `m_center -= QPointF(dx*mpp, -dy*mpp)` (drag right
    moves the map east, so the centre moves west; drag down moves the centre
    north), update; always emit `cursorMoved` with the projected position of
    the cursor (pixel -> visibleExtent: `east = left + x*mpp`,
    `north = bottom - y*mpp`).
  - release: end pan, `OpenHandCursor`.
  - wheel: zoom about the cursor. Compute the projected point under the
    cursor before, multiply mpp by `pow(1.0015, -delta)` clamped, then shift
    `m_center` so that same point stays under the cursor. Accept it.
- `sizeHint()`: `QSize(900, 640)`.
- Set `setMouseTracking(true)` and `setFocusPolicy(Qt::StrongFocus)` in the
  constructor.

Clamp constant: `kMinMpp = 0.25`, `kMaxMpp = 200.0` (named, commented).

## Tests (`tests/test_mapview.cpp`, `QTEST_MAIN`)
1. `emptyViewDoesNotCrash`: construct, resize 320x240, `render()` into a
   `QImage`, succeeds; `metersPerPixel() > 0`.
2. `loadRealLayers` (`QSKIP` if `assets/layers/gale_hirise_ortho_1m.tif`
   missing): `DatasetRegistry reg; reg.loadPrepared(...)`; `MapView v;
   v.setRegistry(&reg);` resize 640x480; `v.fitToScene();` check active id is
   `hirise_optical`, `metersPerPixel()` is within `[0.25, 200]`, and
   `visibleExtentMeters()` intersects the ortho `Info::extent`.
3. `fitShowsWholeLayer`: after fit, the visible extent contains the active
   layer extent (allow 1% slack).
4. `zoomChangesScale`: record mpp, call `zoomIn()`, assert smaller; `zoomOut()`
   twice, assert larger than the original.
5. `panMovesCentre`: resize 400x300, `setMetersPerPixel(2.0)`, record
   `visibleExtentMeters().center()`, simulate press at (200,150), move to
   (240,150), release with `QTest::mousePress/mouseMove/mouseRelease`;
   assert the new centre x is **less** than the old (drag right moves map
   east). Use `QSignalSpy` to assert `viewChanged` and `cursorMoved` fired.
6. `wheelZoomKeepsCursorPoint` (optional but cheap).

Use `QSignalSpy` (`Qt6::Test`). No GUI display: CTest sets
`QT_QPA_PLATFORM=offscreen`.

## CMake
Add to `tests/CMakeLists.txt`:
```cmake
add_executable(test_mapview test_mapview.cpp)
target_link_libraries(test_mapview PRIVATE cetus_core Qt6::Test)
add_test(NAME mapview COMMAND test_mapview)
set_tests_properties(mapview PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;CETUS_ASSET_DIR=${CMAKE_SOURCE_DIR}/assets"
)
```

## Verify
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```
`scaffold` and `raster` must still pass. Report files, commands, test output,
warnings. Do not claim anything not run.
