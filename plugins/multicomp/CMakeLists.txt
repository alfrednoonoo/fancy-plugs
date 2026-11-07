fancy_add_plugin(FancyMultiComp
    PRODUCT_NAME "Fancy MultiComp"
    PLUGIN_CODE FMcp
    BUNDLE_ID com.fancyplugs.multicomp
    SCREEN_MACRO FANCY_HAS_MULTICOMP
    SOURCES
        src/LinearCrossover.cpp
        src/MultiCompDsp.cpp
        src/MultiCompProcessor.cpp
        src/BandGraph.cpp
        src/MultiCompEditor.cpp
    ENTRY src/PluginEntry.cpp)
