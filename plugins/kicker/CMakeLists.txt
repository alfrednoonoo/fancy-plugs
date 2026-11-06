fancy_add_plugin(FancyKicker
    PRODUCT_NAME "Fancy Kicker"
    PLUGIN_CODE FKik
    BUNDLE_ID com.fancyplugs.kicker
    SCREEN_MACRO FANCY_HAS_KICKER
    SOURCES
        src/KickerProcessor.cpp
        src/KickerEditor.cpp
    ENTRY src/PluginEntry.cpp)
