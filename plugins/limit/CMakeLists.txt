fancy_add_plugin(FancyLimit
    PRODUCT_NAME "Fancy Limit"
    PLUGIN_CODE FLim
    BUNDLE_ID com.fancyplugs.limit
    SCREEN_MACRO FANCY_HAS_LIMIT
    SOURCES
        src/LimitDsp.cpp
        src/LimitProcessor.cpp
        src/LimitEditor.cpp
    ENTRY src/PluginEntry.cpp)
