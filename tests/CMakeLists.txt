juce_add_console_app(fancy-tests PRODUCT_NAME "fancy-tests")

get_property(fancy_screen_sources GLOBAL PROPERTY FANCY_SCREEN_SOURCES)
get_property(fancy_screen_includes GLOBAL PROPERTY FANCY_SCREEN_INCLUDES)
get_property(fancy_screen_defs GLOBAL PROPERTY FANCY_SCREEN_DEFS)

target_sources(fancy-tests PRIVATE DecibelsTests.cpp ${fancy_screen_sources})

if("FANCY_HAS_KICKER=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE KickerTests.cpp)
endif()

if("FANCY_HAS_EXCITER=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE ExciterTests.cpp)
endif()

target_include_directories(fancy-tests PRIVATE ${fancy_screen_includes})

target_compile_definitions(fancy-tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(fancy-tests
    PRIVATE
        Catch2::Catch2WithMain
        fancy_ui
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags)

add_test(NAME fancy-tests COMMAND fancy-tests)
