# fancy_core: plain C++ DSP helpers, no JUCE.
add_library(fancy_core INTERFACE)
target_include_directories(fancy_core INTERFACE include)
target_compile_features(fancy_core INTERFACE cxx_std_20)

juce_add_binary_data(FancyFonts
    NAMESPACE FancyFontData
    HEADER_NAME FancyFontData.h
    SOURCES
        ../assets/fonts/Inter-Regular.ttf
        ../assets/fonts/Inter-Medium.ttf
        ../assets/fonts/Inter-SemiBold.ttf)

# fancy_ui: processor base and UI components. Its sources compile into each
# consumer, so they pick up that target's JUCE module settings.
add_library(fancy_ui INTERFACE)
target_include_directories(fancy_ui INTERFACE include)
target_link_libraries(fancy_ui INTERFACE fancy_core FancyFonts)
target_sources(fancy_ui INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/FancyProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/PresetManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Style.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Controls.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Meter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShellEditor.cpp")
