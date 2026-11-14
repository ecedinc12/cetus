# M2c — wire the legend into MainWindow

`MapView` now draws multiple visible layers and `LayerLegend` provides the
toggles. Replace the single-layer combo with the legend in the main window.

## Allowed files
- `src/ui/MainWindow.h`
- `src/ui/MainWindow.cpp`
- `tests/test_ui.cpp`

Do not touch MapView, LayerLegend, CMake, SidePanel, or any other file.

## MainWindow
- Include `ui/LayerLegend.h`; forward-declare `LayerLegend`.
- Remove `QComboBox` / `m_layerCombo` entirely.
- Add `LayerLegend *m_legend = nullptr;` and `LayerLegend *legend() const;`.
- In the constructor, after the side panel dock, add a bottom dock:
  ```cpp
  auto *legendDock = new QDockWidget(tr("Layers"), this);
  legendDock->setObjectName(QStringLiteral("layerLegendDock"));
  m_legend = new LayerLegend(legendDock);
  legendDock->setWidget(m_legend);
  addDockWidget(Qt::BottomDockWidgetArea, legendDock);
  ```
- Toolbar keeps only `Fit`, `+`, `-` (object names `fitButton`,
  `zoomInButton`, `zoomOutButton`).
- On successful load:
  ```cpp
  m_mapView->setRegistry(&m_registry);
  m_legend->setRegistry(&m_registry, m_mapView->visibleLayerIds());
  ```
  Connect `LayerLegend::layerToggled` to
  `m_mapView->setLayerVisible(id, visible)` (lambda).
  Connect `MapView::visibleLayersChanged` to a private
  `void updateStatus()`.
- `updateStatus()` shows in the status bar:
  `SOL 3 · 08:40 · TAU 0.6 · <n> layers · <v> visible`
  and appends ` · NO LAYERS VISIBLE` when `v == 0`.
  Keep the existing loud `RASTER LOAD FAILED: %1` failure message.
- `layersLoaded()` stays true only for the 4 prepared layers.

## Tests (`tests/test_ui.cpp`)
Remove the QComboBox tests/includes. Keep `fitsRasterOnFirstShow`,
`fitAndRender`, `missingAssetsFailLoudly`. Add/update:
1. `loadsPreparedLayers()` — 4 registry layers, `w.legend()` non-null,
   `legend()->layerCount() == 4`, map active `hirise_optical`,
   `map->visibleLayerIds() == {"hirise_optical"}`, optical checkbox checked.
   The slope detail label contains `"HiRISE DTM / MRO"` and `"degrees"`.
2. `legendTogglesLayer()` — QSKIP guard; construct, `resize(640,480)`,
   `show()`, `qWait(20)`; render the map to an image; check slope checkbox
   (`legend()->checkbox("hirise_slope")`) is unchecked; set it checked;
   process events; verify `map->isLayerVisible("hirise_slope")`; render again;
   `QVERIFY(imageBefore != imageAfter)`; uncheck and verify it is hidden.
3. `missingAssetsFailLoudly()` — still fails loudly, legend count 0.

## Verify
```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```
All 5 tests must pass (scaffold, raster, mapview, layerlegend, ui).
