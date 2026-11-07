cmake_minimum_required(VERSION 3.25)

project(FancyPlugs VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT, so the plugins load on a machine without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# "all", or one of the names in FANCY_ALL_PLUGINS. Set by the workflow_dispatch input.
set(FANCY_PLUGINS "all" CACHE STRING "Plugin to build: all, kicker, exciter, comp, eq, verb, delay or multicomp")
set(FANCY_ALL_PLUGINS kicker exciter comp eq verb delay multicomp)

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
