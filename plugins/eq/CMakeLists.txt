fancy_add_plugin(FancyEq
    PRODUCT_NAME "Fancy EQ"
    PLUGIN_CODE FEqu
    BUNDLE_ID com.fancyplugs.eq
    SCREEN_MACRO FANCY_HAS_EQ
    SOURCES
        src/EqFilters.cpp
        src/LinearPhase.cpp
        src/EqDsp.cpp
        src/EqMatch.cpp
        src/EqProcessor.cpp
        src/EqGraph.cpp
        src/EqEditor.cpp
    ENTRY src/PluginEntry.cpp)
