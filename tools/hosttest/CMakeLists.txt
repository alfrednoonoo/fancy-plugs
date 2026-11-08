# Loads the built .vst3 bundles as a host does (Phase 2.1 item H.7.2).
juce_add_console_app(fancy-hosttest PRODUCT_NAME "fancy-hosttest")

target_sources(fancy-hosttest PRIVATE Main.cpp)

target_compile_definitions(fancy-hosttest PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(fancy-hosttest
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags)
