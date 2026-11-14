# M1d — fit the raster on first show

Tiny fix. `MainWindow` calls `MapView::setRegistry()` in its constructor,
before the widget has its final size, so `fitToScene()` fits against the
wrong (default) widget size and the raster can appear tiny at launch.

## Allowed files
- `src/ui/MapView.h`
- `src/ui/MapView.cpp`
- `tests/test_ui.cpp`

Do not touch anything else.

## Change
Add `void showEvent(QShowEvent *event) override;` to `MapView`.
In the implementation:
```cpp
void MapView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_didInitialFit) {
        m_didInitialFit = true;
        if (m_registry && !m_activeLayerId.isEmpty())
            fitToScene();
    }
}
```
Add `bool m_didInitialFit = false;` to the private members. Include
`<QShowEvent>`. Do not refit on later shows.

## Test
In `tests/test_ui.cpp`, extend `loadsPreparedLayers` (or add one slot):
- construct `MainWindow w;`
- `w.resize(960, 700); w.show(); QTest::qWait(20);`
- `auto *map = w.findChild<MapView*>();`
- get the active layer from `w.registry()`; `QVERIFY(map->visibleExtentMeters()
  .contains(layer->info().extent))` with 1% slack, or assert the visible
  extent contains the layer centre and is not more than 3x the layer extent
  per axis (the pre-fix default-size fit would be far larger).
Close the window at the end (`w.close()`).

## Verify
```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```
All four tests must pass. Report the diff and output.
