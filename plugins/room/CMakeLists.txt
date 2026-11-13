fancy_add_plugin(FancyRoom
    PRODUCT_NAME "Fancy Room"
    PLUGIN_CODE FRoo
    BUNDLE_ID com.fancyplugs.room
    SCREEN_MACRO FANCY_HAS_ROOM
    SOURCES
        src/RoomDsp.cpp
        src/RoomProcessor.cpp
        src/RoomEditor.cpp
    ENTRY src/PluginEntry.cpp)
