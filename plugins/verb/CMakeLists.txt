# Phase 3 Q2.8: the IR library is made during the build (docs/verb.md, Decision 2 A). A console
# tool runs the plugin's own generators on library/recipes.json and packs the IRs as FLAC into
# one file, which goes into the plugin as a Windows resource, or on macOS through the assembler.
juce_add_console_app(verb-library PRODUCT_NAME "verb-library")

target_sources(verb-library PRIVATE
    "${CMAKE_SOURCE_DIR}/tools/verb-library/Main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/VerbChain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/IrBuilder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/VerbLibrary.cpp"
    "${CMAKE_SOURCE_DIR}/fancy-core/src/MatchedFilters.cpp")

# Phase 4 Wave 1: the IR builder's EQ module uses fancy-core's matched filters.
target_include_directories(verb-library PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/fancy-core/include")

target_compile_definitions(verb-library PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(verb-library
    PRIVATE
        juce::juce_audio_formats
        juce::juce_data_structures
        juce::juce_dsp
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

if(WIN32)
    file(WRITE "${verb_library_rc}" "FANCYVERBLIBRARY RCDATA \"${verb_library_bin}\"\n")
    set_source_files_properties("${verb_library_rc}" PROPERTIES OBJECT_DEPENDS "${verb_library_bin}")
    set(verb_library_source "${verb_library_rc}")
else()
    # macOS (docs/mac.md): the same file, embedded with the assembler's .incbin. A hash of every
    # file the library is made from goes into the source, so a compiler cache can't reuse an
    # object holding an older library.
    file(GLOB verb_library_inputs
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*"
        "${CMAKE_CURRENT_SOURCE_DIR}/library/*"
        "${CMAKE_SOURCE_DIR}/tools/verb-library/*")
    list(SORT verb_library_inputs)
    set(verb_library_hashes "")
    foreach(input IN LISTS verb_library_inputs)
        file(SHA256 "${input}" input_hash)
        string(APPEND verb_library_hashes "${input_hash}")
    endforeach()
    string(SHA256 verb_library_hash "${verb_library_hashes}")

    set(verb_library_source "${verb_generated_dir}/VerbLibraryEmbed.cpp")
    string(CONFIGURE [=[
// Written by plugins/verb/CMakeLists.txt: Fancy Verb's IR library, embedded with the assembler.
#include <juce_core/juce_core.h>

extern "C" const unsigned char fancyVerbLibraryStart[];
extern "C" const unsigned char fancyVerbLibraryEnd[];

__asm__ (".section __TEXT,__const\n"
         ".globl _fancyVerbLibraryStart\n"
         ".p2align 4\n"
         "_fancyVerbLibraryStart:\n"
         ".incbin \"@verb_library_bin@\"\n"
         ".globl _fancyVerbLibraryEnd\n"
         "_fancyVerbLibraryEnd:\n"
         ".byte 0\n"
         ".text\n");

namespace verb
{
extern const char* const embeddedLibraryHash;
const char* const embeddedLibraryHash = "@verb_library_hash@";

juce::MemoryBlock embeddedLibraryData()
{
    return juce::MemoryBlock (fancyVerbLibraryStart, (size_t) (fancyVerbLibraryEnd - fancyVerbLibraryStart));
}
} // namespace verb
]=] verb_library_embed_text @ONLY)
    file(WRITE "${verb_library_source}" "${verb_library_embed_text}")
    set_source_files_properties("${verb_library_source}" PROPERTIES OBJECT_DEPENDS "${verb_library_bin}")
endif()

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
        src/LibraryPicker.cpp
        src/VerbEditor.cpp
    ENTRY src/PluginEntry.cpp)

# The library goes into the binaries that run the plugin's code: the plugin's own formats,
# and fancy-screens and fancy-tests, which compile the same sources.
foreach(format_target FancyVerb_VST3 FancyVerb_AU FancyVerb_Standalone)
    if(TARGET ${format_target})
        target_sources(${format_target} PRIVATE "${verb_library_source}")
        add_dependencies(${format_target} verb-library-data)
    endif()
endforeach()

add_dependencies(FancyVerb verb-library-data)

set_property(GLOBAL APPEND PROPERTY FANCY_SCREEN_SOURCES "${verb_library_source}")
set_property(GLOBAL APPEND PROPERTY FANCY_SCREEN_DEPENDS verb-library-data)
