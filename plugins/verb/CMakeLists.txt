# Phase 3 Q2.8: the IR library is made during the build (docs/verb.md, Decision 2 A). A console
# tool runs the plugin's own generators on library/recipes.json and packs the IRs as FLAC into
# one file, which goes into the plugin as a Windows resource.
juce_add_console_app(verb-library PRODUCT_NAME "verb-library")

target_sources(verb-library PRIVATE
    "${CMAKE_SOURCE_DIR}/tools/verb-library/Main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/VerbChain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/IrBuilder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/VerbLibrary.cpp")

target_include_directories(verb-library PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_compile_definitions(verb-library PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(verb-library
    PRIVATE
        juce::juce_audio_formats
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags)

set(verb_generated_dir "${CMAKE_BINARY_DIR}/generated/verb")
set(verb_library_bin "${verb_generated_dir}/VerbLibrary.bin")
set(verb_library_rc "${verb_generated_dir}/VerbLibrary.rc")

add_custom_command(
    OUTPUT "${verb_library_bin}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${verb_generated_dir}"
    COMMAND verb-library "${CMAKE_CURRENT_SOURCE_DIR}/library/recipes.json" "${verb_library_bin}"
    DEPENDS verb-library "${CMAKE_CURRENT_SOURCE_DIR}/library/recipes.json"
    COMMENT "Building Fancy Verb's IR library"
    VERBATIM)

add_custom_target(verb-library-data DEPENDS "${verb_library_bin}")

file(WRITE "${verb_library_rc}" "FANCYVERBLIBRARY RCDATA \"${verb_library_bin}\"\n")
set_source_files_properties("${verb_library_rc}" PROPERTIES OBJECT_DEPENDS "${verb_library_bin}")

fancy_add_plugin(FancyVerb
    PRODUCT_NAME "Fancy Verb"
    PLUGIN_CODE FVrb
    BUNDLE_ID com.fancyplugs.verb
    SCREEN_MACRO FANCY_HAS_VERB
    SOURCES
        src/Convolver.cpp
        src/IrBuilder.cpp
        src/IrFiles.cpp
        src/VerbChain.cpp
        src/VerbLibrary.cpp
        src/VerbDsp.cpp
        src/VerbProcessor.cpp
        src/IrDisplay.cpp
        src/ModuleRack.cpp
        src/VerbEditor.cpp
    ENTRY src/PluginEntry.cpp)

# The resource goes into the binaries that run the plugin's code: the plugin's own formats,
# and fancy-screens and fancy-tests, which compile the same sources.
foreach(format_target FancyVerb_VST3 FancyVerb_Standalone)
    if(TARGET ${format_target})
        target_sources(${format_target} PRIVATE "${verb_library_rc}")
        add_dependencies(${format_target} verb-library-data)
    endif()
endforeach()

add_dependencies(FancyVerb verb-library-data)

set_property(GLOBAL APPEND PROPERTY FANCY_SCREEN_SOURCES "${verb_library_rc}")
set_property(GLOBAL APPEND PROPERTY FANCY_SCREEN_DEPENDS verb-library-data)
