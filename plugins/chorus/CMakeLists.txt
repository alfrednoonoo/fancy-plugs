# Fancy Chorus, a chorus and modulation effect (docs/chorus.md).
fancy_add_plugin(FancyChorus
    PRODUCT_NAME "Fancy Chorus"
    PLUGIN_CODE FCho
    BUNDLE_ID com.fancyplugs.chorus
    SCREEN_MACRO FANCY_HAS_CHORUS
    SOURCES
        src/ChorusDsp.cpp
        src/ChorusProcessor.cpp
        src/ChorusEditor.cpp
    ENTRY src/PluginEntry.cpp)
