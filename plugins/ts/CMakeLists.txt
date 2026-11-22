# Fancy TS, a transient shaper (docs/ts.md).
fancy_add_plugin(FancyTS
    PRODUCT_NAME "Fancy TS"
    PLUGIN_CODE FTsh
    BUNDLE_ID com.fancyplugs.ts
    SCREEN_MACRO FANCY_HAS_TS
    SOURCES
        src/TsDsp.cpp
        src/TsProcessor.cpp
        src/TsEditor.cpp
    ENTRY src/PluginEntry.cpp)
