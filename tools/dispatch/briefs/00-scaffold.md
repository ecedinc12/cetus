# M0 — scaffold

Implement the CMake scaffold and an empty application window. No science logic yet.

## Allowed paths
- `CMakeLists.txt`
- `src/main.cpp`
- `src/app/MissionState.{h,cpp}`
- `src/data/DatasetRegistry.{h,cpp}`
- `src/data/RasterLayer.{h,cpp}`
- `src/gis/PlanetaryCrs.{h,cpp}`
- `src/analysis/HazardMap.{h,cpp}`
- `src/analysis/TargetRanking.{h,cpp}`
- `src/routing/TraversePlanner.{h,cpp}`
- `src/ui/MainWindow.{h,cpp}`
- `src/ui/MapView.{h,cpp}`
- `src/ui/SidePanel.{h,cpp}`
- `tests/CMakeLists.txt`
- `tests/test_scaffold.cpp`

Do not touch anything else. Do not create the files this brief does not list.

## Requirements
1. `cmake_minimum_required(VERSION 3.20)`, C++17, `CMAKE_AUTOMOC ON`.
2. `find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)` and `find_package(GDAL REQUIRED)`.
3. Target `cetus` builds from `src/main.cpp` and links `Qt6::Widgets` and `GDAL::GDAL`.
4. `main.cpp` creates a `QApplication`, shows a `MainWindow` with a central `MapView`
   placeholder, a `SidePanel` placeholder, and a status bar with static text
   `SOL 3 · 08:40 · TAU 0.6`.
5. `PlanetaryCrs` exposes the Mars body radius constant `3396190.0` and a
   `marsEquirectangular(double centralMeridianDegrees)` function returning an
   EPSG-less PROJ string. Unit test it.
6. `MissionState` is a plain struct with sol, clock, tau, and consumables; no logic.
7. `tests/` registers at least one `ctest` test that links `Qt6::Test` and checks
   `PlanetaryCrs`.
8. The tree must build clean with `-Wall -Wextra`.

## Verify
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/cetus   # may need QT_QPA_PLATFORM=offscreen
```

Report: files created, commands run, and the exact test output.
