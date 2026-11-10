fancy_add_plugin(FancyDSMini
    PRODUCT_NAME "Fancy DS-Mini"
    PLUGIN_CODE FDsm
    BUNDLE_ID com.fancyplugs.dsmini
    SCREEN_MACRO FANCY_HAS_DSMINI
    SOURCES
        src/DsMiniProcessor.cpp
        src/DsMiniEditor.cpp
    ENTRY src/PluginEntry.cpp)
