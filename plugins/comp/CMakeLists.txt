fancy_add_plugin(FancyComp
    PRODUCT_NAME "Fancy Comp"
    PLUGIN_CODE FCmp
    BUNDLE_ID com.fancyplugs.comp
    SCREEN_MACRO FANCY_HAS_COMP
    SOURCES
        src/CompDsp.cpp
        src/CompProcessor.cpp
        src/CompMeters.cpp
        src/CompEditor.cpp
    ENTRY src/PluginEntry.cpp)
