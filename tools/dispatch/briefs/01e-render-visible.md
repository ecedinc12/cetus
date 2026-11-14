# M1e — prove the real raster actually renders

Test-only change. Add one assertion that the map canvas is not blank when a
real prepared layer is loaded.

## Allowed file
- `tests/test_mapview.cpp`

Do not touch anything else.

## Change
Add a test slot `realRasterRendersPixels()`:
- `QSKIP` unless `hasPreparedAssets()`.
- Load `DatasetRegistry`, `MapView view; view.setRegistry(&reg);
  view.resize(640, 480); view.fitToScene();`
- Render into `QImage img(view.size(), QImage::Format_ARGB32_Premultiplied)`,
  `img.fill(QColor(0,0,0)); view.render(&img);`
- Count pixels that differ from the canvas background `QColor(24,18,15)` by
  more than 12 in any of R/G/B. Count the whole image (overlays are small).
- `QVERIFY(nonBackground > img.width() * img.height() / 5);` — at least 20%.
  A blank/failed raster read leaves the background colour and fails.
- Also `QVERIFY(view.metersPerPixel() > 0.0);`

Name the private helper if useful. No network, no new fixture.

## Verify
```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```
All four tests must pass; report the new test output.
