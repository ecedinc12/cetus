# M1c — wire the map into MainWindow

One small integration step. `MapView` and the raster core are finished and
tested. Do not change them. Read only `src/ui/MainWindow.h`,
`src/ui/MainWindow.cpp`, `src/ui/MapView.h`, `src/data/DatasetRegistry.h`.
Start writing now.

## Allowed files
- `src/ui/MainWindow.h`
- `src/ui/MainWindow.cpp`
- `tests/test_ui.cpp` (new)
- `tests/CMakeLists.txt`

Do not touch `MapView`, `SidePanel`, `src/data/`, `src/gis/`, `src/app/`,
`src/main.cpp`, root `CMakeLists.txt`, `assets/`, `data/`, `docs/`, `tools/`,
`logs/`, or Markdown.

## MainWindow API (add, keep the constructor argument)
```cpp
const DatasetRegistry &registry() const;
bool layersLoaded() const;
const QString &loadError() const;
```
Members: keep `MapView *m_mapView`, `SidePanel *m_sidePanel`; add
`DatasetRegistry m_registry; QString m_loadError;`.

## Constructor behaviour
1. Keep title `Cetus — Mars Surface Field Tool` and `resize(1280, 800)`.
2. `setCentralWidget(m_mapView)` as today; keep the `Field Panel` dock.
3. Create a `QToolBar` (object name `mapToolBar`, title `Map`) at the top:
   - `QComboBox` object name `layerCombo`, minimum width 220. Do not populate
     it until loading succeeds.
   - `QToolButton`/`QPushButton` object name `fitButton`, text `Fit`.
   - `QToolButton` object name `zoomInButton`, text `+`;
     `QToolButton` object name `zoomOutButton`, text `-`.
4. Load:
   ```cpp
   QString error;
   if (!m_registry.loadPrepared(&error)) {
       m_loadError = error;
       statusBar()->showMessage(QStringLiteral("RASTER LOAD FAILED: %1").arg(error));
       qCritical().noquote() << "Cetus: raster load failed:" << error;
   } else {
       for (const auto &layer : m_registry.layers())
           m_layerCombo->addItem(layer->metadata().title,
                                 layer->metadata().id);
       m_mapView->setRegistry(&m_registry);
       const int idx = m_layerCombo->findData(m_mapView->activeLayerId());
       if (idx >= 0) m_layerCombo->setCurrentIndex(idx);
       statusBar()->showMessage(QStringLiteral("SOL 3 · 08:40 · TAU 0.6 · 4 layers"));
   }
   ```
   No modal dialog: the app must stay usable and fail loudly in the status bar.
5. Connections:
   - combo `currentIndexChanged(int)` -> `m_mapView->setActiveLayer(
     combo->currentData().toString())`.
   - fit button -> `m_mapView->fitToScene()`.
   - zoom buttons -> `zoomIn()` / `zoomOut()`.
   - `MapView::cursorMoved` -> update a permanent `QLabel` (object name
     `cursorLabel`) in the status bar: `E %1 m · N %2 m` with no decimals.
   - `MapView::viewChanged` -> update `cursorLabel`? No; leave cursor label to
     `cursorMoved`. Do not add more.
6. `layersLoaded()` returns `m_registry.layers().size() == 4`; `registry()`
   returns `m_registry`; `loadError()` returns `m_loadError`.

Use `QToolBar::addWidget` and `QStatusBar::addPermanentWidget`. Include what
you use. No hard-coded layer titles: take them from metadata.

## Tests (`tests/test_ui.cpp`, `QTEST_MAIN`)
1. `loadsPreparedLayers`: QSKIP unless
   `assets/layers/gale_hirise_ortho_1m.tif` exists (use `SiteConfig::layerPath`
   or the same existence check as `test_mapview`). Construct `MainWindow`;
   `QVERIFY(w.layersLoaded()); QCOMPARE(w.registry().layers().size(), 4);`
   `QCOMPARE(w.loadError(), QString());`
   `auto *combo = w.findChild<QComboBox*>("layerCombo"); QVERIFY(combo);`
   `QCOMPARE(combo->count(), 4);`
   `auto *map = w.findChild<MapView*>(); QVERIFY(map);`
   `QCOMPARE(map->activeLayerId(), QString("hirise_optical"));`
2. `comboSwitchesLayer`: find index whose `itemData` is `hirise_slope`, set it,
   `QCoreApplication::processEvents()`, assert map active id is
   `hirise_slope`.
3. `fitAndRender`: click `fitButton` via `QTest::mouseClick`, resize 640x480,
   `QVERIFY(map->metersPerPixel() > 0)`, `map->render(&image)` produces a
   non-null image; no crash.
4. `missingAssetsFailLoudly`: set `qputenv("CETUS_ASSET_DIR", "/nonexistent")`,
   construct `MainWindow`, check `!layersLoaded()` and `!loadError().isEmpty()`;
   restore the previous value (use `qgetenv`/`qputenv`). Do not show dialogs.

Include `ui/MainWindow.h`, `ui/MapView.h`, `data/DatasetRegistry.h`.
No network. Use `QTEST_MAIN` and offscreen.

## CMake
Add in `tests/CMakeLists.txt`:
```cmake
add_executable(test_ui test_ui.cpp)
target_link_libraries(test_ui PRIVATE cetus_core Qt6::Test)
add_test(NAME ui COMMAND test_ui)
set_tests_properties(ui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;CETUS_ASSET_DIR=${CMAKE_SOURCE_DIR}/assets"
)
```

## Verify
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```
All four tests (`scaffold`, `raster`, `mapview`, `ui`) must pass. Report
files, commands, output, warnings. Do not claim anything not run.
