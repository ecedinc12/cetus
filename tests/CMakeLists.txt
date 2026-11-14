find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_scaffold test_scaffold.cpp)
target_link_libraries(test_scaffold PRIVATE cetus_core Qt6::Test)

add_test(NAME scaffold COMMAND test_scaffold)

# Raster/CRS/registry tests exercise the GDAL API directly when building
# fixture rasters, so link GDAL::GDAL explicitly as well.
add_executable(test_raster test_raster.cpp)
target_link_libraries(test_raster PRIVATE cetus_core Qt6::Test GDAL::GDAL)

add_test(NAME raster COMMAND test_raster)
set_tests_properties(raster PROPERTIES
    ENVIRONMENT "CETUS_ASSET_DIR=${CMAKE_SOURCE_DIR}/assets"
)

add_executable(test_mapview test_mapview.cpp)
target_link_libraries(test_mapview PRIVATE cetus_core Qt6::Test)
add_test(NAME mapview COMMAND test_mapview)
set_tests_properties(mapview PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;CETUS_ASSET_DIR=${CMAKE_SOURCE_DIR}/assets"
)

add_executable(test_ui test_ui.cpp)
target_link_libraries(test_ui PRIVATE cetus_core Qt6::Test)
add_test(NAME ui COMMAND test_ui)
set_tests_properties(ui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;CETUS_ASSET_DIR=${CMAKE_SOURCE_DIR}/assets"
)
