fancy_add_plugin(FancyFir
    PRODUCT_NAME "Fancy FIR"
    PLUGIN_CODE FFir
    BUNDLE_ID com.fancyplugs.fir
    SCREEN_MACRO FANCY_HAS_FIR
    SOURCES
        src/SpectralEngine.cpp
        src/FirDsp.cpp
        src/FirProcessor.cpp
        src/FirGraph.cpp
        src/FirEditor.cpp
    ENTRY src/PluginEntry.cpp)
