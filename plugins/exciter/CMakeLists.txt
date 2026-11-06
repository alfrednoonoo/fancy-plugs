fancy_add_plugin(FancyExciter
    PRODUCT_NAME "Fancy Exciter"
    PLUGIN_CODE FExc
    BUNDLE_ID com.fancyplugs.exciter
    SCREEN_MACRO FANCY_HAS_EXCITER
    SOURCES
        src/ExciterDsp.cpp
        src/ExciterProcessor.cpp
        src/ExciterEditor.cpp
    ENTRY src/PluginEntry.cpp)
