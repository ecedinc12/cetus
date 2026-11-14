# Brief 07d — make the whole MAP KEY visible at 1280×800

07a added the MAP KEY / DATA NOTE column, but its rich text is taller than the
bottom dock: measured `mapKeyNote` is `360x198` while `heightForWidth(360)` is
`258`. The DATA NOTE (rotated HiRISE footprint, black no-data wedges, grid
north) is cut off — exactly the explanation the crew needs. Do not change the
meanings, colours, hazard logic or the layer rows.

## Files you may change

- `src/ui/LayerLegend.cpp` (the map-key rich text and its label sizing only)
- `tests/test_ui.cpp` (one new fit assertion)

Do not touch `LayerLegend.h`, `HazardMap`, `MapView`, `SidePanel`,
`MainWindow`, assets, data, docs, tools, logs or root Markdown.

## Changes

1. Rewrite `m_mapKeyNote`'s rich text as **three compact paragraphs with
   `style="margin:0"`** (no blank paragraphs, no `<br/>` inside a paragraph):

   ```html
   <p style="margin:0"><b>HAZARD</b> — derived 10 m grid from HiRISE slope.
   <span style="color:#dc1e14">NO-GO</span> &gt;20° ·
   <span style="color:#9b59c0">UNKNOWN</span> no data = no-go ·
   <span style="color:#ffaa00">CAUTION</span> 10–20°</p>
   <p style="margin:0"><b style="color:#00dcff">ROUTE</b> solid ·
   <b style="color:#00dcff">PLAN</b> dashed ·
   <b style="color:#50d25a">START</b> ·
   <b style="color:#ffbe3c">TARGET</b> ·
   <b style="color:#ff8c00">NEXT</b></p>
   <p style="margin:0"><b>DATA NOTE</b> — HiRISE strip is rotated in the
   north-up Mars grid; black wedges = no data; arrow = grid north.</p>
   ```

   Keep every concept and colour from 07a. Keep the wording `NO-GO`,
   `UNKNOWN`, `CAUTION`, `ROUTE`, `PLAN`, `START`, `TARGET`, `NEXT`,
   `DATA NOTE`, `rotated`, `north-up` and `no data` so existing tests and the
   user-facing explanation still hold. Keep font `11px`, word wrap, min width
   `360`, max width `460`.

2. Constrain the label so it can never force or exceed the dock:
   `m_mapKeyNote->setMinimumHeight(0);` and set a size policy with
   `QSizePolicy::Preferred` horizontally and `QSizePolicy::MinimumExpanding`
   vertically. Do not give it a fixed height.

3. Do not raise the bottom dock height or lower `MapView`'s minimum height.

## Test

Add `void mapKeyFitsTheDock();` to `tests/test_ui.cpp::TestMainWindow`:

- construct `MainWindow`, `resize(1280, 800)`, `show()`, let layout settle
  (`QApplication::processEvents()` is enough in this suite);
- find `QLabel *key = w.findChild<QLabel *>("mapKeyNote")`;
- require `key->text().contains("DATA NOTE")`;
- require the rendered height covers the wrapped text:
  `key->height() >= key->heightForWidth(key->width())`;
- require `key->mapTo(&w, QPoint(0, key->height())).y() <= w.height()` so the
  last line is inside the window.

## Verification

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Report the exact totals. No screenshot claim.
