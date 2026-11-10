fancy_add_plugin(FancyDS
    PRODUCT_NAME "Fancy DS"
    PLUGIN_CODE FDes
    BUNDLE_ID com.fancyplugs.ds
    SCREEN_MACRO FANCY_HAS_DS
    SOURCES
        src/DsProcessor.cpp
        src/DsEditor.cpp
    ENTRY src/PluginEntry.cpp)
