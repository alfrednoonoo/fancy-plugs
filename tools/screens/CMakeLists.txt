juce_add_console_app(fancy-screens PRODUCT_NAME "fancy-screens")

get_property(fancy_screen_sources GLOBAL PROPERTY FANCY_SCREEN_SOURCES)
get_property(fancy_screen_includes GLOBAL PROPERTY FANCY_SCREEN_INCLUDES)
get_property(fancy_screen_defs GLOBAL PROPERTY FANCY_SCREEN_DEFS)

target_sources(fancy-screens PRIVATE Main.cpp ${fancy_screen_sources})
target_include_directories(fancy-screens PRIVATE ${fancy_screen_includes})

target_compile_definitions(fancy-screens PRIVATE
    ${fancy_screen_defs}
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(fancy-screens
    PRIVATE
        fancy_ui
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
