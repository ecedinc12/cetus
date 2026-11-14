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

add_executable(test_layerlegend test_layerlegend.cpp)
target_link_libraries(test_layerlegend PRIVATE cetus_core Qt6::Test)
add_test(NAME layerlegend COMMAND test_layerlegend)
set_tests_properties(layerlegend PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;CETUS_ASSET_DIR=${CMAKE_SOURCE_DIR}/assets"
)

add_executable(test_analysis test_analysis.cpp)
target_link_libraries(test_analysis PRIVATE cetus_core Qt6::Test)
add_test(NAME analysis COMMAND test_analysis)
set_tests_properties(analysis PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;CETUS_ASSET_DIR=${CMAKE_SOURCE_DIR}/assets"
)

add_executable(test_routing test_routing.cpp)
target_link_libraries(test_routing PRIVATE cetus_core Qt6::Test)
add_test(NAME routing COMMAND test_routing)
set_tests_properties(routing PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;CETUS_ASSET_DIR=${CMAKE_SOURCE_DIR}/assets"
)

add_executable(test_sidepanel test_sidepanel.cpp)
target_link_libraries(test_sidepanel PRIVATE cetus_core Qt6::Test)
add_test(NAME sidepanel COMMAND test_sidepanel)
set_tests_properties(sidepanel PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;CETUS_ASSET_DIR=${CMAKE_SOURCE_DIR}/assets"
)
