cmake_minimum_required(VERSION 3.23)
project(tpl VERSION 1.0.0 LANGUAGES CXX)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(AW_TPL_IS_TOP_LEVEL ON)
else()
    set(AW_TPL_IS_TOP_LEVEL OFF)
endif()

option(AW_TPL_BUILD_EXAMPLES "Build aw::tpl example applications" ${AW_TPL_IS_TOP_LEVEL})

find_package(Threads REQUIRED)

add_library(aw_tpl STATIC
    src/thread_pool.cpp
)
add_library(aw::tpl ALIAS aw_tpl)

target_compile_features(aw_tpl PUBLIC cxx_std_20)

target_sources(aw_tpl
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/aw/tpl/tpl.hpp
            include/aw/tpl/thread_pool.hpp
            include/aw/tpl/task.hpp
            include/aw/tpl/when_all.hpp
            include/aw/tpl/when_any.hpp
            include/aw/tpl/cancellation.hpp
)

target_link_libraries(aw_tpl PUBLIC Threads::Threads)
option(AW_TPL_ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

if(AW_TPL_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(aw_tpl PUBLIC --coverage -O0 -g)
    target_link_options(aw_tpl PUBLIC --coverage)
endif()


# --- Install & Package Export Rules ---
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

if(AW_TPL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

option(AW_TPL_BUILD_TESTS "Build aw::tpl unit tests" ${AW_TPL_IS_TOP_LEVEL})

if(AW_TPL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
