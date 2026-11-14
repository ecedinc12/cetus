# M3a fix — align the analysis test with the corrected degree-slope layer

The M1 slope product was rebuilt. It previously used `gdaldem slope -p`
(percent slope) but was labelled degrees. The corrected product is now true
degrees (range 0.003–70.401°), and the real sampled values at the pins are:

- START 5.58°
- Pahrump Hills (T1) 2.90°
- Marias Pass (T2) 13.52°
- Vera Rubin Ridge (T3) 7.49°

So the test assumption that Marias Pass is a >20° no-go is wrong. Fix only the
test; do not change product code, data, docs, or thresholds.

## Allowed files
- `tests/test_analysis.cpp`

## Change

Rename/replace the `mariasPassIsNoGo()` slot with
`allTargetPinsAreTraversable()`:

- Build the hazard grid at 20 m as before.
- Assert `!hazard.isNoGoAt(...)` for all four pins:
  START `(-753.3972146034672, -276224.80223393487)`,
  T1 `(-756.135485790287, -276363.057354002)`,
  T2 `(-1102.35727505766, -276690.662501919)`,
  T3 `(-1285.075, -279807.988)`.
- Assert `hazard.cost(...)` is finite at each point's cell. To get the cell,
  derive `cellWidth = extentMeters().width()/width()` and
  `cellHeight = extentMeters().height()/height()`, clamp the point to a cell,
  and query `cost(col,row)`. Keep the helper local to the test.
- Also prove the threshold still blocks: find any cell with
  `slopeDeg(col,row) > 20.0` and assert `isNoGo(col,row)` and
  `!std::isfinite(cost(col,row))`. Skip the `QSKIP` only if no such cell exists
  in the real raster (it does).
- Update the slot declaration and the comment: the Marias pin is a **13.5°**
  real slope, not 24°.

## Verify
```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```
The `analysis` test and all existing tests must pass. Report the commands run.
