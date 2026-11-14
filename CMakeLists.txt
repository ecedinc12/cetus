cmake_minimum_required(VERSION 3.20)
project(cetus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(GDAL REQUIRED)

# Shared library so tests can link the same code the app ships.
add_library(cetus_core STATIC
    src/app/MissionState.h
    src/app/MissionState.cpp
    src/data/DatasetRegistry.h
    src/data/DatasetRegistry.cpp
    src/data/RasterLayer.h
    src/data/RasterLayer.cpp
    src/data/SiteConfig.h
    src/data/SiteConfig.cpp
    src/gis/PlanetaryCrs.h
    src/gis/PlanetaryCrs.cpp
    src/analysis/HazardMap.h
    src/analysis/HazardMap.cpp
    src/analysis/TargetRanking.h
    src/analysis/TargetRanking.cpp
    src/routing/TraversePlanner.h
    src/routing/TraversePlanner.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/MapView.h
    src/ui/MapView.cpp
    src/ui/SidePanel.h
    src/ui/SidePanel.cpp
)
target_include_directories(cetus_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
# Prepared assets ship next to the sources; tests/app can override at
# runtime with the CETUS_ASSET_DIR environment variable.
target_compile_definitions(cetus_core PUBLIC
    CETUS_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
target_link_libraries(cetus_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        GDAL::GDAL
)

add_executable(cetus src/main.cpp)
target_link_libraries(cetus
    PRIVATE
        cetus_core
        Qt6::Widgets
        GDAL::GDAL
)

enable_testing()
add_subdirectory(tests)
