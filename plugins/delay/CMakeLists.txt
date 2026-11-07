fancy_add_plugin(FancyDelay
    PRODUCT_NAME "Fancy Delay"
    PLUGIN_CODE FDly
    BUNDLE_ID com.fancyplugs.delay
    SCREEN_MACRO FANCY_HAS_DELAY
    SOURCES
        src/DelayDsp.cpp
        src/DelayProcessor.cpp
        src/TapMap.cpp
        src/DelayEditor.cpp
    ENTRY src/PluginEntry.cpp)
