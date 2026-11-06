# fancy_add_plugin(<target>
#     PRODUCT_NAME "Fancy Kicker"
#     PLUGIN_CODE  FKik
#     BUNDLE_ID    com.fancyplugs.kicker
#     SCREEN_MACRO FANCY_HAS_KICKER
#     SOURCES      src/KickerProcessor.cpp src/KickerEditor.cpp
#     ENTRY        src/PluginEntry.cpp)
#
# SOURCES hold the processor and editor classes and are also compiled into
# fancy-screens, which renders the editors without loading a plugin binary.
# ENTRY holds createPluginFilter() and goes into the plugin only, because each
# plugin defines that same symbol.
function(fancy_add_plugin target)
    cmake_parse_arguments(ARG "" "PRODUCT_NAME;PLUGIN_CODE;BUNDLE_ID;SCREEN_MACRO;ENTRY" "SOURCES" ${ARGN})

    juce_add_plugin(${target}
        PRODUCT_NAME "${ARG_PRODUCT_NAME}"
        COMPANY_NAME "Fancy Plugs"
        BUNDLE_ID "${ARG_BUNDLE_ID}"
        PLUGIN_MANUFACTURER_CODE Fncy
        PLUGIN_CODE ${ARG_PLUGIN_CODE}
        FORMATS VST3 Standalone
        VST3_CATEGORIES Fx
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    list(TRANSFORM ARG_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

    target_sources(${target} PRIVATE ${ARG_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_ENTRY}")

    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(${target}
        PRIVATE
            fancy_core
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    set_property(GLOBAL APPEND PROPERTY FANCY_SCREEN_SOURCES ${ARG_SOURCES})
    set_property(GLOBAL APPEND PROPERTY FANCY_SCREEN_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/src")
    set_property(GLOBAL APPEND PROPERTY FANCY_SCREEN_DEFS "${ARG_SCREEN_MACRO}=1")
endfunction()
