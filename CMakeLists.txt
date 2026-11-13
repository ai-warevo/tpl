cmake_minimum_required(VERSION 3.23)
# Single source of truth for the version; written by scripts/release.sh / release.ps1.
file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" AW_TPL_VERSION)
string(STRIP "${AW_TPL_VERSION}" AW_TPL_VERSION)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/VERSION")
project(tpl VERSION ${AW_TPL_VERSION} LANGUAGES CXX)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(AW_TPL_IS_TOP_LEVEL ON)
else()
    set(AW_TPL_IS_TOP_LEVEL OFF)
endif()

if(AW_TPL_IS_TOP_LEVEL)
    # Lets clangd / clang-tidy / IDEs find the exact flags; harmless for consumers (top-level only).
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

option(AW_TPL_BUILD_EXAMPLES "Build aw::tpl example applications" ${AW_TPL_IS_TOP_LEVEL})

find_package(Threads REQUIRED)

# Fail early with a readable message when the toolchain lacks the C++20 library/coroutine support we need.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
    set(CMAKE_REQUIRED_FLAGS "-std=c++20 -fcoroutines")
    set(AW_TPL_NEEDS_FCOROUTINES ON)
elseif(MSVC)
    set(CMAKE_REQUIRED_FLAGS "/std:c++20 /EHsc")
else()
    set(CMAKE_REQUIRED_FLAGS "-std=c++20")
endif()
set(CMAKE_REQUIRED_LIBRARIES Threads::Threads)
check_cxx_source_compiles("
#include <coroutine>
#include <semaphore>
#include <stop_token>
#include <thread>
struct T {
    struct promise_type {
        T get_return_object() { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};
T coro() { co_return; }
int main() {
    std::binary_semaphore sem{0};
    std::jthread th([](std::stop_token st) { (void)st.stop_requested(); });
    sem.release();
    coro();
}" AW_TPL_HAS_CXX20_RUNTIME)
unset(CMAKE_REQUIRED_FLAGS)
unset(CMAKE_REQUIRED_LIBRARIES)
if(NOT AW_TPL_HAS_CXX20_RUNTIME)
    message(FATAL_ERROR "aw::tpl needs a C++20 toolchain with coroutines, std::jthread, std::stop_token and std::binary_semaphore "
                        "(GCC >= 11, Clang >= 14 with a recent libc++/libstdc++, or MSVC 2022).")
endif()

add_library(aw_tpl STATIC
    src/operation_canceled_exception.cpp
    src/timeout_exception.cpp
    src/threading/cancellation_token.cpp
    src/threading/cancellation_token_source.cpp
    src/threading/thread_pool.cpp
    src/threading/detail/cancellation_state.cpp
    src/threading/detail/delay_awaiter.cpp
    src/threading/tasks/async_sleep.cpp
    src/threading/tasks/task_already_moved_exception.cpp
    src/threading/tasks/task_void.cpp
    src/threading/tasks/detail/cancelable_sleep_awaiter.cpp
    src/threading/tasks/detail/completion_awaiter.cpp
    src/threading/tasks/detail/final_awaiter.cpp
    src/threading/tasks/detail/initial_schedule_awaiter.cpp
    src/threading/tasks/detail/promise_void.cpp
    src/threading/tasks/detail/task_state_base.cpp
    src/threading/tasks/detail/when_any_awaiter.cpp
    src/threading/tasks/detail/when_any_state.cpp
    src/threading/tasks/detail/when_any_watcher.cpp
)
add_library(aw::tpl ALIAS aw_tpl)

target_compile_features(aw_tpl PUBLIC cxx_std_20)

option(AW_TPL_WARNINGS_AS_ERRORS "Build the aw_tpl library with strict warnings treated as errors" OFF)
if(AW_TPL_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(aw_tpl PRIVATE /W4 /WX)
    else()
        target_compile_options(aw_tpl PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)
    endif()
endif()

target_sources(aw_tpl
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/aw/tpl/operation_canceled_exception.hpp
            include/aw/tpl/timeout_exception.hpp
            include/aw/tpl/tpl.hpp
            include/aw/tpl/threading/cancellation_token.hpp
            include/aw/tpl/threading/cancellation_token_source.hpp
            include/aw/tpl/threading/thread_pool.hpp
            include/aw/tpl/threading/detail/cancellation_state.hpp
            include/aw/tpl/threading/detail/delay_awaiter.hpp
            include/aw/tpl/threading/detail/detached.hpp
            include/aw/tpl/threading/detail/pool_scheduling.hpp
            include/aw/tpl/threading/detail/timer_entry.hpp
            include/aw/tpl/threading/tasks/async_sleep.hpp
            include/aw/tpl/threading/tasks/task.hpp
            include/aw/tpl/threading/tasks/task_already_moved_exception.hpp
            include/aw/tpl/threading/tasks/task_fwd.hpp
            include/aw/tpl/threading/tasks/task_void.hpp
            include/aw/tpl/threading/tasks/when_all.hpp
            include/aw/tpl/threading/tasks/when_any.hpp
            include/aw/tpl/threading/tasks/detail/cancelable_sleep_awaiter.hpp
            include/aw/tpl/threading/tasks/detail/completion_awaiter.hpp
            include/aw/tpl/threading/tasks/detail/final_awaiter.hpp
            include/aw/tpl/threading/tasks/detail/initial_schedule_awaiter.hpp
            include/aw/tpl/threading/tasks/detail/is_truly_copyable.hpp
            include/aw/tpl/threading/tasks/detail/promise.hpp
            include/aw/tpl/threading/tasks/detail/promise_base.hpp
            include/aw/tpl/threading/tasks/detail/promise_void.hpp
            include/aw/tpl/threading/tasks/detail/run_async.hpp
            include/aw/tpl/threading/tasks/detail/task_state.hpp
            include/aw/tpl/threading/tasks/detail/task_state_base.hpp
            include/aw/tpl/threading/tasks/detail/when_all_element.hpp
            include/aw/tpl/threading/tasks/detail/when_all_settled.hpp
            include/aw/tpl/threading/tasks/detail/when_any_awaiter.hpp
            include/aw/tpl/threading/tasks/detail/when_any_state.hpp
            include/aw/tpl/threading/tasks/detail/when_any_watcher.hpp
)

target_link_libraries(aw_tpl PUBLIC Threads::Threads)
if(AW_TPL_NEEDS_FCOROUTINES)
    target_compile_options(aw_tpl PUBLIC -fcoroutines)
endif()
option(AW_TPL_ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

if(AW_TPL_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(aw_tpl PUBLIC --coverage -O0 -g -fprofile-update=atomic)
    target_link_options(aw_tpl PUBLIC --coverage)
endif()

# Sanitizer builds (PUBLIC so that tests and consumers are instrumented as well).
set(AW_TPL_SANITIZE "none" CACHE STRING "Sanitizer to enable: none, address (ASan+UBSan) or thread (TSan)")
set_property(CACHE AW_TPL_SANITIZE PROPERTY STRINGS none address thread)

if(NOT AW_TPL_SANITIZE STREQUAL "none")
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "AW_TPL_SANITIZE requires GCC or Clang")
    endif()
    if(AW_TPL_ENABLE_COVERAGE)
        message(FATAL_ERROR "AW_TPL_SANITIZE cannot be combined with AW_TPL_ENABLE_COVERAGE")
    endif()
    if(AW_TPL_SANITIZE STREQUAL "address")
        set(AW_TPL_SANITIZE_FLAGS "address,undefined")
    elseif(AW_TPL_SANITIZE STREQUAL "thread")
        set(AW_TPL_SANITIZE_FLAGS "thread")
    else()
        message(FATAL_ERROR "AW_TPL_SANITIZE must be one of: none, address, thread")
    endif()
    target_compile_options(aw_tpl PUBLIC -fsanitize=${AW_TPL_SANITIZE_FLAGS} -fno-omit-frame-pointer -g)
    target_link_options(aw_tpl PUBLIC -fsanitize=${AW_TPL_SANITIZE_FLAGS})
endif()


# --- Install & Package Export Rules ---
option(AW_TPL_INSTALL "Generate install rules and package config for aw::tpl" ${AW_TPL_IS_TOP_LEVEL})

if(AW_TPL_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS aw_tpl
        EXPORT aw_tplTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT aw_tplTargets
        FILE aw_tplTargets.cmake
        NAMESPACE aw::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aw_tpl
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/aw_tplConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/aw_tplConfig.cmake"
        "include(CMakeFindDependencyMacro)\n"
        "find_dependency(Threads)\n"
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/aw_tplTargets.cmake\")\n"
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/aw_tplConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/aw_tplConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aw_tpl
    )
endif()

if(AW_TPL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

option(AW_TPL_BUILD_TESTS "Build aw::tpl unit tests" ${AW_TPL_IS_TOP_LEVEL})

if(AW_TPL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
