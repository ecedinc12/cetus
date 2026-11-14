# M2b — LayerLegend widget

Add a bottom legend widget with one checkbox per prepared layer. Each row must
name the layer, its source, resolution, and units. Toggling a checkbox emits a
signal; the widget never touches GDAL.

## Allowed files
- `src/ui/LayerLegend.h` (new)
- `src/ui/LayerLegend.cpp` (new)
- `tests/test_layerlegend.cpp` (new)
- `CMakeLists.txt`
- `tests/CMakeLists.txt`

Do not touch MainWindow, MapView, SidePanel, or any other file.

## Header
```cpp
#pragma once
#include "data/RasterLayer.h"
#include <QStringList>
#include <QWidget>
class DatasetRegistry;
class QCheckBox;
class QLabel;

class LayerLegend : public QWidget
{
    Q_OBJECT
public:
    explicit LayerLegend(QWidget *parent = nullptr);

    // Non-owning registry. Rebuilds the rows. `visibleIds` sets initial checks.
    void setRegistry(const DatasetRegistry *registry,
                     const QStringList &visibleIds = {});
    int layerCount() const;
    bool isLayerVisible(const QString &layerId) const;
    QStringList visibleLayerIds() const;
    QCheckBox *checkbox(const QString &layerId) const;
    QLabel *detailLabel(const QString &layerId) const;

public slots:
    void setLayerVisible(const QString &layerId, bool visible);

signals:
    void layerToggled(const QString &layerId, bool visible);
};
```

## UI
- Title `LAYERS` (bold, ~16 px) and a small hint:
  `check to show · source · resolution · units`.
- One row per registry layer, in registry order. Row = `QCheckBox` (layer
  `title`, 14 px, minimum width 190) + detail `QLabel`
  `source · resolutionText · units` (12 px, muted colour).
- Object names: `layerToggle_<id>` for the checkbox,
  `layerDetail_<id>` for the detail label.
- Tooltip: `product`.
- Checked state comes from `visibleIds`.
- `setRegistry(nullptr, ...)` clears all rows.
- `setLayerVisible()` updates the checkbox with `QSignalBlocker`; it must not
  emit `layerToggled`. Unknown ids are ignored.
- User toggle emits `layerToggled(id, checked)` exactly once.
- Rows rebuild cleanly if `setRegistry()` is called twice.

## CMake
- Add `src/ui/LayerLegend.h` and `src/ui/LayerLegend.cpp` to `cetus_core`.
- Add `test_layerlegend` target linked to `cetus_core` and `Qt6::Test`,
  test name `layerlegend`, environment
  `QT_QPA_PLATFORM=offscreen;CETUS_ASSET_DIR=${CMAKE_SOURCE_DIR}/assets`.
- Do not remove or rename existing targets.

## Tests (`tests/test_layerlegend.cpp`)
1. `registryBuildsRows()` — QSKIP if `assets/layers/gale_hirise_ortho_1m.tif`
   missing; load `DatasetRegistry`; set optical as the only visible id;
   `layerCount() == 4`; optical checkbox checked, slope unchecked; each detail
   label contains the layer's `source`, `resolutionText`, and `units`.
2. `toggleEmitsOnce()` — `QSignalSpy`; set slope checkbox checked; exactly one
   `layerToggled("hirise_slope", true)`; `visibleLayerIds()` contains it;
   `setLayerVisible("hirise_slope", false)` unchecks it without another signal.
3. `unknownIdIgnored()` — no crash, row count unchanged.
4. `emptyRegistryClearsRows()` — `setRegistry(nullptr)` then `layerCount() == 0`.

## Verify in this repository copy
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```
All existing tests plus `layerlegend` must pass.
