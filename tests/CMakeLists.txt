add_executable(fancy-tests
    DecibelsTests.cpp)

target_link_libraries(fancy-tests PRIVATE fancy_core Catch2::Catch2WithMain)

add_test(NAME fancy-tests COMMAND fancy-tests)
