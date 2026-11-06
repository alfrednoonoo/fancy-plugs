# Pinned third-party dependencies. CI's dependency cache key hashes this file,
# so changing a tag here also refreshes the cache.
include(FetchContent)

# 8.0.17-security is the newest tag on the JUCE 8 line (BUILD_PROMPT §1 asks for JUCE 8).
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.17-security
    GIT_SHALLOW TRUE)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.1
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(JUCE Catch2)
