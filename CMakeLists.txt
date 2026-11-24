cmake_minimum_required(VERSION 3.23)

include(FetchContent)
FetchContent_Declare(
    aw_cmake_modules
    GIT_REPOSITORY https://github.com/ai-warevo/template_cpp
    GIT_TAG main
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(aw_cmake_modules)
list(APPEND CMAKE_MODULE_PATH "${aw_cmake_modules_SOURCE_DIR}/cmake")

include(AWProject)
aw_project_prepare(NAME tpl NAMESPACE aw:: VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
project(${AW_PROJECT_NAME} VERSION ${AW_PROJECT_VERSION} LANGUAGES CXX)
aw_project_setup(PREFIX AW_TPL BUILD_TESTS BUILD_EXAMPLES INSTALL)

find_package(Threads REQUIRED)
aw_check_cxx23_runtime()

include(DependencyCache)

set(AW_TPL_SOURCES
    src/operation_canceled_exception.cpp
    src/timeout_exception.cpp
    src/threading/cancellation_token.cpp
    src/threading/cancellation_token_source.cpp
    src/threading/thread_pool.cpp
    src/threading/detail/cancellation_state.cpp
    src/threading/detail/delay_awaiter.cpp
    src/threading/tasks/async_sleep.cpp
    src/threading/tasks/task_already_moved_exception.cpp
    src/threading/tasks/task_group.cpp
    src/threading/tasks/task_void.cpp
    src/threading/tasks/detail/cancelable_sleep_awaiter.cpp
    src/threading/tasks/detail/completion_awaiter.cpp
    src/threading/tasks/detail/final_awaiter.cpp
    src/threading/tasks/detail/initial_schedule_awaiter.cpp
    src/threading/tasks/detail/promise_void.cpp
    src/threading/tasks/detail/task_state_base.cpp
    src/threading/tasks/detail/when_any_awaiter.cpp
    src/threading/tasks/detail/when_any_state.cpp
    src/threading/tasks/detail/when_any_watcher.cpp)

set(AW_TPL_HEADERS
    include/aw/tpl/operation_canceled_exception.hpp
    include/aw/tpl/timeout_exception.hpp
    include/aw/tpl/tpl.hpp
    include/aw/tpl/threading/cancellation_token.hpp
    include/aw/tpl/threading/cancellation_token_source.hpp
    include/aw/tpl/threading/thread_pool.hpp
    include/aw/tpl/threading/detail/callback_function.hpp
    include/aw/tpl/threading/detail/cancellation_state.hpp
    include/aw/tpl/threading/detail/delay_awaiter.hpp
    include/aw/tpl/threading/detail/detached.hpp
    include/aw/tpl/threading/detail/pool_scheduling.hpp
    include/aw/tpl/threading/detail/timer_entry.hpp
    include/aw/tpl/threading/tasks/async_sleep.hpp
    include/aw/tpl/threading/tasks/spawn.hpp
    include/aw/tpl/threading/tasks/task.hpp
    include/aw/tpl/threading/tasks/task_already_moved_exception.hpp
    include/aw/tpl/threading/tasks/task_fwd.hpp
    include/aw/tpl/threading/tasks/task_group.hpp
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
    include/aw/tpl/threading/tasks/detail/spawn_guard.hpp
    include/aw/tpl/threading/tasks/detail/task_state.hpp
    include/aw/tpl/threading/tasks/detail/task_group_awaiter.hpp
    include/aw/tpl/threading/tasks/detail/task_state_base.hpp
    include/aw/tpl/threading/tasks/detail/when_all_element.hpp
    include/aw/tpl/threading/tasks/detail/when_all_settled.hpp
    include/aw/tpl/threading/tasks/detail/when_any_awaiter.hpp
    include/aw/tpl/threading/tasks/detail/when_any_state.hpp
    include/aw/tpl/threading/tasks/detail/when_any_watcher.hpp)

aw_add_library(TARGET aw_tpl TYPE STATIC ALIAS aw::tpl
    SOURCES ${AW_TPL_SOURCES}
    HEADERS ${AW_TPL_HEADERS}
    PUBLIC_INCLUDE_DIRS include
    PUBLIC_LINKS Threads::Threads)

if(AW_TPL_WARNINGS_AS_ERRORS)
    aw_configure_compiler(TARGET aw_tpl STRICT_WARNINGS)
endif()

if(AW_ENABLE_COVERAGE)
    # Add coverage flags to library target for code coverage analysis
    target_compile_options(aw_tpl PRIVATE --coverage)
    target_link_options(aw_tpl PRIVATE --coverage)
endif()

if(AW_TPL_INSTALL)
    aw_install_library(TARGET aw_tpl NAME tpl NS aw::tpl DEPENDENCIES Threads)
endif()

if(AW_TPL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
aw_setup_tests(OPTION AW_TPL_BUILD_TESTS DIRECTORY tests)
