# FancyBench, the benchmark host (Phase 3 item Q1, docs/phase3-Q1.md). Loads any VST3 and measures
# it; ships as tools/FancyBench.exe in the build artifact.
juce_add_console_app(fancy-bench PRODUCT_NAME "FancyBench")

target_sources(fancy-bench PRIVATE
    src/Main.cpp
    src/Analysis.cpp
    src/TestFile.cpp
    src/Host.cpp
    src/Report.cpp)

target_compile_definitions(fancy-bench PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(fancy-bench
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags)
