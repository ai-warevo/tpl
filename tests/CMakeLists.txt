include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(Catch2)

add_executable(aw_tpl_tests
    test_thread_pool.cpp
    test_task.cpp
    test_when_all.cpp
    test_when_any.cpp
    test_cancellation.cpp
)

target_link_libraries(aw_tpl_tests PRIVATE
    aw::tpl
    Catch2::Catch2WithMain
)

include(CTest)
if(WIN32)
    # Windows keeps a single ctest entry (per-test discovery was not verified there); add a timeout
    # so that a deadlock fails the run instead of hanging it.
    add_test(NAME aw_tpl_tests COMMAND aw_tpl_tests)
    set_tests_properties(aw_tpl_tests PROPERTIES TIMEOUT 300)
else()
    include(Catch)
    catch_discover_tests(aw_tpl_tests PROPERTIES TIMEOUT 120)
endif()
