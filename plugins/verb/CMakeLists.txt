fancy_add_plugin(FancyVerb
    PRODUCT_NAME "Fancy Verb"
    PLUGIN_CODE FVrb
    BUNDLE_ID com.fancyplugs.verb
    SCREEN_MACRO FANCY_HAS_VERB
    SOURCES
        src/Convolver.cpp
        src/IrBuilder.cpp
        src/IrFiles.cpp
        src/VerbChain.cpp
        src/VerbDsp.cpp
        src/VerbProcessor.cpp
        src/IrDisplay.cpp
        src/ModuleRack.cpp
        src/VerbEditor.cpp
    ENTRY src/PluginEntry.cpp)
