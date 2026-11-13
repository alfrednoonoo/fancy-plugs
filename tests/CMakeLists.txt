juce_add_console_app(fancy-tests PRODUCT_NAME "fancy-tests")

get_property(fancy_screen_sources GLOBAL PROPERTY FANCY_SCREEN_SOURCES)
get_property(fancy_screen_includes GLOBAL PROPERTY FANCY_SCREEN_INCLUDES)
get_property(fancy_screen_defs GLOBAL PROPERTY FANCY_SCREEN_DEFS)

target_sources(fancy-tests PRIVATE DecibelsTests.cpp ValueEntryTests.cpp PopupTests.cpp PresetTests.cpp ListenTests.cpp HostRenderTests.cpp HostTests.cpp WorkflowTests.cpp ${fancy_screen_sources})

# FancyBench's analysis and test-file code (Phase 3 Q1), tested without its hosting.
target_sources(fancy-tests PRIVATE BenchTests.cpp
    ${CMAKE_SOURCE_DIR}/tools/bench/src/Analysis.cpp
    ${CMAKE_SOURCE_DIR}/tools/bench/src/TestFile.cpp)
target_include_directories(fancy-tests PRIVATE ${CMAKE_SOURCE_DIR}/tools/bench/src)

if("FANCY_HAS_KICKER=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE KickerTests.cpp KickerSubTests.cpp)
endif()

if("FANCY_HAS_EXCITER=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE ExciterTests.cpp LegacyExciterDsp.cpp)
endif()

if("FANCY_HAS_COMP=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE CompTests.cpp)
endif()

if("FANCY_HAS_EQ=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE EqTests.cpp DynamicEqTests.cpp)
endif()

if("FANCY_HAS_VERB=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE VerbTests.cpp)
endif()

if("FANCY_HAS_DELAY=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE DelayTests.cpp)
endif()

if("FANCY_HAS_MULTICOMP=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE MultiCompTests.cpp UpwardTests.cpp)
endif()

if("FANCY_HAS_FIR=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE FirTests.cpp AdaptiveTests.cpp)
endif()

if("FANCY_HAS_GATE=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE GateTests.cpp)
endif()

if("FANCY_HAS_LIMIT=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE LimitTests.cpp)
endif()

if("FANCY_HAS_DS=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE DsTests.cpp)
endif()

if("FANCY_HAS_DSMINI=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE DsMiniTests.cpp)
endif()

if("FANCY_HAS_SATURE=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE SatureTests.cpp)
endif()

if("FANCY_HAS_ROOM=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE RoomTests.cpp)
endif()

# Headers generated during the build, such as Fancy Sature's auto-gain table.
get_property(fancy_screen_depends GLOBAL PROPERTY FANCY_SCREEN_DEPENDS)
if(fancy_screen_depends)
    add_dependencies(fancy-tests ${fancy_screen_depends})
endif()

target_include_directories(fancy-tests PRIVATE ${fancy_screen_includes})

target_compile_definitions(fancy-tests PRIVATE
    ${fancy_screen_defs}
    FANCY_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(fancy-tests
    PRIVATE
        Catch2::Catch2WithMain
        fancy_ui
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags)

add_test(NAME fancy-tests COMMAND fancy-tests)
