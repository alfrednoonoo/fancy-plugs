fancy_add_plugin(FancyGate
    PRODUCT_NAME "Fancy Gate"
    PLUGIN_CODE FGat
    BUNDLE_ID com.fancyplugs.gate
    SCREEN_MACRO FANCY_HAS_GATE
    PRODUCES_MIDI
    SOURCES
        src/GateDsp.cpp
        src/GateProcessor.cpp
        src/GateEditor.cpp
    ENTRY src/PluginEntry.cpp)
