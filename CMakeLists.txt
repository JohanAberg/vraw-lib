cmake_minimum_required(VERSION 3.16)

project(vraw VERSION 2.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(VRAW_BUILD_EXAMPLES "Build example programs" ON)
option(VRAW_BUILD_TESTS "Build tests" OFF)

# VRAW library (includes LZ4 source directly)
add_library(vraw STATIC
    src/VrawWriter.cpp
    src/VrawReader.cpp
    src/Encoding.cpp
    src/lz4/lz4.c
)

# Also build shared library
add_library(vraw_shared SHARED
    src/VrawWriter.cpp
    src/VrawReader.cpp
    src/Encoding.cpp
    src/lz4/lz4.c
)

target_include_directories(vraw
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lz4
)

target_include_directories(vraw_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lz4
)

# No external link dependencies - lz4 is compiled in

# Set output name for shared library
set_target_properties(vraw_shared PROPERTIES OUTPUT_NAME vraw)

# Platform-specific settings
if(APPLE)
    # macOS/iOS
    target_compile_options(vraw PRIVATE -Wall -Wextra)
    target_compile_options(vraw_shared PRIVATE -Wall -Wextra)
elseif(UNIX)
    # Linux
    target_compile_options(vraw PRIVATE -Wall -Wextra)
    target_compile_options(vraw_shared PRIVATE -Wall -Wextra)
    target_link_libraries(vraw PRIVATE m)
    target_link_libraries(vraw_shared PRIVATE m)
elseif(WIN32)
    # Windows
    target_compile_definitions(vraw PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(vraw_shared PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Examples
if(VRAW_BUILD_EXAMPLES)
    add_executable(vraw_example examples/example.cpp)
    target_link_libraries(vraw_example PRIVATE vraw)

    add_executable(vraw_info examples/vraw_info.cpp)
    target_link_libraries(vraw_info PRIVATE vraw)
endif()

# Tests
if(VRAW_BUILD_TESTS)
    enable_testing()

    add_executable(test_variants tests/test_variants.cpp)
    target_link_libraries(test_variants PRIVATE vraw)

    add_test(NAME variant_tests COMMAND test_variants)
endif()

# Install rules
include(GNUInstallDirs)

# Don't export lz4 - it's statically linked into vraw
install(TARGETS vraw vraw_shared
    EXPORT vrawTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT vrawTargets
    FILE vrawTargets.cmake
    NAMESPACE vraw::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vraw
)

# Package config
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/vrawConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/vrawConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/vrawConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vraw
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/vrawConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/vrawConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vraw
)

# Summary
message(STATUS "")
message(STATUS "VRAW Library ${PROJECT_VERSION}")
message(STATUS "  Build examples: ${VRAW_BUILD_EXAMPLES}")
message(STATUS "  Build tests: ${VRAW_BUILD_TESTS}")
message(STATUS "")
