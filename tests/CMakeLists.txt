include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com
    GIT_TAG        v3.7.1
)

FetchContent_Declare(
    FakeIt
    GIT_REPOSITORY https://github.com
    GIT_TAG        2.4.1
)

FetchContent_MakeAvailable(Catch2 FakeIt)

add_executable(aw_tpl_tests
    test_thread_pool.cpp
    test_task.cpp
    test_when_all.cpp
    test_when_any.cpp
    test_cancellation.cpp
)

target_include_directories(aw_tpl_tests PRIVATE
    ${fakeit_SOURCE_DIR}/single_header/catch
)

target_link_libraries(aw_tpl_tests PRIVATE
    aw::tpl
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(aw_tpl_tests)
