# M2a — MapView multi-layer visibility

Add multi-layer visibility to the map canvas. The current `MapView` draws only
the active layer. M2 needs several layers visible at once, toggled by id.

## Allowed files
- `src/ui/MapView.h`
- `src/ui/MapView.cpp`
- `tests/test_mapview.cpp`

Do not touch CMake, MainWindow, SidePanel, or any other file.

## Header additions
```cpp
#include <QStringList>

public:
    void setLayerVisible(const QString &layerId, bool visible);
    bool isLayerVisible(const QString &layerId) const;
    // IDs in bottom-to-top draw order.
    QStringList visibleLayerIds() const;
    void showAllLayers();
    void hideAllLayers();

signals:
    void visibleLayersChanged();
```
Private member `QStringList m_visibleLayerIds;`.

## Behaviour
- `setRegistry()`: clear visibility, make the default layer active and the only
  visible layer, then fit. `nullptr` clears everything.
- `setActiveLayer(id)`: keep M1 semantics — active layer **and solo** visible.
- `setLayerVisible(id, visible)`: ignore null registry / unknown id. Add or
  remove the id. Emit `visibleLayersChanged()` only when the set changed.
  Emit `update()` always.
- `showAllLayers()`: every registry id visible (draw order still applies).
- `hideAllLayers()`: none visible.
- `fitToScene()`: fit the union of **visible** layer extents. If none visible,
  fall back to active, then all registry extents.
- If no registry or no visible layers: still draw the dark background plus
  north arrow / scale bar, and the centred text `"MAP VIEW — no layers loaded"`
  (no registry) or `"NO LAYERS VISIBLE"` (registry but all hidden). Never crash.

## Draw order and opacity
Sort visible layers by `RasterLayer::Kind`:
`Optical` (bottom) → `Hillshade` → `Elevation` → `Slope` (top).
For each visible layer, draw `readImage(visibleExtentMeters(), size())` as-is
(north-up, never flip). Use `QPainter::setOpacity`:
- first visible layer: `1.0`
- `Hillshade` above something: `0.55`
- `Elevation` / `Slope` above something: `0.50`
- reset opacity to `1.0` before the overlays (north arrow, scale bar,
  attribution).
Attribution text stays the **active** layer's
`source · resolutionText · units`; if the active layer is hidden or missing,
use the topmost visible layer's metadata.

## Tests (`tests/test_mapview.cpp`)
Keep every existing test passing. Add:
1. `layerTogglesAffectRendering()` — real assets (QSKIP guard), 480×360,
   fit; render image A; `setLayerVisible("hirise_slope", true)`; verify
   `visibleLayerIds()` contains optical and slope, slope after optical; render
   image B; `QVERIFY(imageA != imageB)`.
2. `soloActiveHidesOthers()` — `setActiveLayer("hirise_slope")` leaves only
   slope visible and active.
3. `hideAllLayersLeavesEmptyView()` — `hideAllLayers()`; ids empty; render
   does not crash; `setLayerVisible("hirise_slope", true)` then ids == slope.
4. `unknownLayerIdIsIgnored()` — `setLayerVisible("no_such_layer", true)`
   leaves ids unchanged and does not crash.

## Verify in this repository copy
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```
All 4 existing tests must still pass (scaffold, raster, mapview, ui).
