juce_add_console_app(fancy-tests PRODUCT_NAME "fancy-tests")

get_property(fancy_screen_sources GLOBAL PROPERTY FANCY_SCREEN_SOURCES)
get_property(fancy_screen_includes GLOBAL PROPERTY FANCY_SCREEN_INCLUDES)
get_property(fancy_screen_defs GLOBAL PROPERTY FANCY_SCREEN_DEFS)

target_sources(fancy-tests PRIVATE TestStartListener.cpp DecibelsTests.cpp PitchShifterTests.cpp ValueEntryTests.cpp PopupTests.cpp PresetTests.cpp ListenTests.cpp HostRenderTests.cpp HostTests.cpp WorkflowTests.cpp ${fancy_screen_sources})

# FancyBench's analysis and test-file code (Phase 3 Q1), tested without its hosting.
# PortabilityTests.cpp checks the platform layer (docs/mac.md, step 1).
target_sources(fancy-tests PRIVATE BenchTests.cpp PortabilityTests.cpp
    ${CMAKE_SOURCE_DIR}/tools/bench/src/Analysis.cpp
    ${CMAKE_SOURCE_DIR}/tools/bench/src/Platform.cpp
    ${CMAKE_SOURCE_DIR}/tools/bench/src/TestFile.cpp)
target_include_directories(fancy-tests PRIVATE ${CMAKE_SOURCE_DIR}/tools/bench/src)

if("FANCY_HAS_KICKER=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE KickerTests.cpp KickerSubTests.cpp KickerW1Tests.cpp KickerW2Tests.cpp)
endif()

if("FANCY_HAS_EXCITER=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE ExciterTests.cpp ExciterW1Tests.cpp ExciterW2bTests.cpp LegacyExciterDsp.cpp LegacyExciterW1Dsp.cpp)
endif()

if("FANCY_HAS_COMP=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE CompTests.cpp CompQ25Tests.cpp CompW1Tests.cpp CompW2Tests.cpp CompOpticalTests.cpp CompSceqTests.cpp LegacyCompDsp.cpp LegacyCompQ25Dsp.cpp LegacyCompW2Dsp.cpp)
endif()

if("FANCY_HAS_EQ=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE EqTests.cpp DynamicEqTests.cpp EqMatchTests.cpp EqW1Tests.cpp EqW2Tests.cpp EqGroupsTests.cpp EqW3Tests.cpp LegacyEqDsp.cpp LegacyEqW2Dsp.cpp)
endif()

if("FANCY_HAS_VERB=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE VerbTests.cpp VerbQ28Tests.cpp VerbW1Tests.cpp VerbDecayTests.cpp VerbDuckTests.cpp VerbW2bTests.cpp VerbW3Tests.cpp LegacyVerbW2Dsp.cpp LegacyVerbConvolver.cpp LegacyVerbIrBuilder.cpp)
endif()

if("FANCY_HAS_DELAY=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE DelayTests.cpp DelayQ23Tests.cpp DelayW1Tests.cpp DelayW2Tests.cpp DelayW3Tests.cpp LegacyDelayDsp.cpp LegacyDelayW1Dsp.cpp LegacyDelayW2Dsp.cpp)
endif()

if("FANCY_HAS_MULTICOMP=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE MultiCompTests.cpp UpwardTests.cpp MultiCompQ26Tests.cpp MultiCompW1Tests.cpp MultiCompW2Tests.cpp MultiCompW2bTests.cpp LegacyMultiCompDsp.cpp LegacyMultiCompW2Dsp.cpp)
endif()

if("FANCY_HAS_FIR=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE FirTests.cpp AdaptiveTests.cpp FirQ27Tests.cpp FirW1Tests.cpp FirW2Tests.cpp FirW2bTests.cpp StftFramesTests.cpp LegacyFirEngine.cpp LegacyFirW2Engine.cpp)
endif()

if("FANCY_HAS_GATE=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE GateTests.cpp GateW1Tests.cpp GateW2Tests.cpp GateW2bTests.cpp LegacyGateDsp.cpp)
endif()

if("FANCY_HAS_LIMIT=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE LimitTests.cpp LimitW1Tests.cpp LimitW2Tests.cpp LegacyLimitDsp.cpp)
endif()

if("FANCY_HAS_DS=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE DsTests.cpp DsW1Tests.cpp DsW2Tests.cpp DsW2bTests.cpp LegacyDeEsser.cpp)
endif()

if("FANCY_HAS_DSMINI=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE DsMiniTests.cpp DsMiniW1Tests.cpp DsMiniW2bTests.cpp)
endif()

# The v1.7.0 de-esser engine, for both plugins' Wave 2 (items 32 to 34) legacy tests.
if("FANCY_HAS_DS=1" IN_LIST fancy_screen_defs OR "FANCY_HAS_DSMINI=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE LegacyDeEsserW2.cpp)
endif()

if("FANCY_HAS_SATURE=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE SatureTests.cpp SatureW2Tests.cpp SatureW2bTests.cpp LegacySatureDsp.cpp LegacySatureW2Dsp.cpp)
endif()

if("FANCY_HAS_TS=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE TsTests.cpp)
endif()

if("FANCY_HAS_CHORUS=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE ChorusTests.cpp)
endif()

if("FANCY_HAS_ROOM=1" IN_LIST fancy_screen_defs)
    target_sources(fancy-tests PRIVATE RoomTests.cpp RoomW1Tests.cpp RoomW2Tests.cpp LegacyRoomDsp.cpp LegacyRoomW1Dsp.cpp)
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
