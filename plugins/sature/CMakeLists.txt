# The auto-gain table is made during the build (docs/sature.md, Decision 6): a small console
# tool runs Fancy Sature's curves from fancy-core and writes SatureGainTable.h, so the table
# always matches them.
add_executable(sature-gaintable "${CMAKE_SOURCE_DIR}/tools/sature-gaintable/Main.cpp")
target_link_libraries(sature-gaintable PRIVATE fancy_core)

set(sature_generated_dir "${CMAKE_BINARY_DIR}/generated/sature")
set(sature_gain_table "${sature_generated_dir}/SatureGainTable.h")

add_custom_command(
    OUTPUT "${sature_gain_table}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${sature_generated_dir}"
    COMMAND sature-gaintable "${sature_gain_table}"
    DEPENDS sature-gaintable "${CMAKE_SOURCE_DIR}/fancy-core/include/fancy/dsp/Saturation.h"
    COMMENT "Generating Fancy Sature's auto-gain table"
    VERBATIM)

add_custom_target(sature-gaintable-header DEPENDS "${sature_gain_table}")

fancy_add_plugin(FancySature
    PRODUCT_NAME "Fancy Sature"
    PLUGIN_CODE FSat
    BUNDLE_ID com.fancyplugs.sature
    SCREEN_MACRO FANCY_HAS_SATURE
    SOURCES
        src/SatureDsp.cpp
        src/SatureProcessor.cpp
        src/SatureEditor.cpp
    ENTRY src/PluginEntry.cpp)

target_include_directories(FancySature PRIVATE "${sature_generated_dir}")
add_dependencies(FancySature sature-gaintable-header)

# fancy-screens and fancy-tests compile the same sources.
set_property(GLOBAL APPEND PROPERTY FANCY_SCREEN_INCLUDES "${sature_generated_dir}")
set_property(GLOBAL APPEND PROPERTY FANCY_SCREEN_DEPENDS sature-gaintable-header)
