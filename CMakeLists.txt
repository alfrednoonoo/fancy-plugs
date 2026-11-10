cmake_minimum_required(VERSION 3.25)

project(FancyPlugs VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT, so the plugins load on a machine without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# "all", or one of the names in FANCY_ALL_PLUGINS. Set by the workflow_dispatch input.
set(FANCY_PLUGINS "all" CACHE STRING "Plugin to build: all, kicker, exciter, comp, eq, verb, delay, multicomp, fir, gate, limit or ds")
set(FANCY_ALL_PLUGINS kicker exciter comp eq verb delay multicomp fir gate limit ds)

include(cmake/Dependencies.cmake)
include(cmake/FancyPlugin.cmake)

add_subdirectory(fancy-core)

foreach(plugin IN LISTS FANCY_ALL_PLUGINS)
    if((FANCY_PLUGINS STREQUAL "all" OR FANCY_PLUGINS STREQUAL plugin)
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugins/${plugin}/CMakeLists.txt")
        add_subdirectory(plugins/${plugin})
    endif()
endforeach()

get_property(fancy_screen_defs GLOBAL PROPERTY FANCY_SCREEN_DEFS)
if(NOT fancy_screen_defs)
    message(FATAL_ERROR "FANCY_PLUGINS='${FANCY_PLUGINS}' selected no plugin that exists yet")
endif()

enable_testing()
add_subdirectory(tests)
add_subdirectory(tools/screens)
add_subdirectory(tools/hosttest)
add_subdirectory(tools/bench)

# JUCE's AudioPluginHost, for checking sidechain and MIDI routing by hand (Phase 2.1 H.7.3).
# Only built when asked for: CI builds it once per JUCE version and caches it.
add_subdirectory("${juce_SOURCE_DIR}/extras/AudioPluginHost" "${CMAKE_BINARY_DIR}/AudioPluginHost" EXCLUDE_FROM_ALL)
