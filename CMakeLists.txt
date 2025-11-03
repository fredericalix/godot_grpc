cmake_minimum_required(VERSION 3.20)
project(godot_grpc VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(USE_VCPKG "Use vcpkg for dependency management" ON)
option(USE_CONAN "Use Conan for dependency management" OFF)

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(GODOT_PLATFORM "ios")
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64")
    endif()
    set(GODOT_ARCH "${CMAKE_OSX_ARCHITECTURES}")
elseif(APPLE)
    set(GODOT_PLATFORM "macos")
    # Build for native architecture only (universal binaries require vcpkg to build for both architectures)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        execute_process(
            COMMAND uname -m
            OUTPUT_VARIABLE NATIVE_ARCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        set(CMAKE_OSX_ARCHITECTURES "${NATIVE_ARCH}")
    endif()
    set(GODOT_ARCH "${CMAKE_OSX_ARCHITECTURES}")
elseif(WIN32)
    set(GODOT_PLATFORM "windows")
    set(GODOT_ARCH "x86_64")
elseif(UNIX)
    set(GODOT_PLATFORM "linux")
    set(GODOT_ARCH "x86_64")
endif()

# Build configuration suffix
# For multi-config generators (Visual Studio), use generator expressions
# For single-config generators (Make, Ninja), use CMAKE_BUILD_TYPE
if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator (Visual Studio, Xcode)
    set(GODOT_BUILD_TYPE "$<IF:$<CONFIG:Debug>,template_debug,template_release>")
else()
    # Single-config generator (Make, Ninja)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(GODOT_BUILD_TYPE "template_debug")
    else()
        set(GODOT_BUILD_TYPE "template_release")
    endif()
endif()

# Library name
set(LIBRARY_NAME "godot_grpc")
set(OUTPUT_NAME "${LIBRARY_NAME}.${GODOT_PLATFORM}.${GODOT_BUILD_TYPE}.${GODOT_ARCH}")

# Dependency management
if(USE_VCPKG)
    message(STATUS "Using vcpkg for dependency management")
    find_package(gRPC CONFIG REQUIRED)
    find_package(Protobuf CONFIG REQUIRED)
    find_package(absl CONFIG REQUIRED)
    set(GRPC_LIBRARIES gRPC::grpc++ gRPC::grpc)
    set(PROTOBUF_LIBRARIES protobuf::libprotobuf)
elseif(USE_CONAN)
    message(STATUS "Using Conan for dependency management")
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup(TARGETS)
    set(GRPC_LIBRARIES CONAN_PKG::grpc)
    set(PROTOBUF_LIBRARIES CONAN_PKG::protobuf)
else()
    message(FATAL_ERROR "Either USE_VCPKG or USE_CONAN must be enabled")
endif()

# godot-cpp
set(GODOT_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/godot-cpp" CACHE PATH "Path to godot-cpp")
if(NOT EXISTS "${GODOT_CPP_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "godot-cpp not found at ${GODOT_CPP_DIR}. Please clone it:\n"
                        "git clone https://github.com/godotengine/godot-cpp ${GODOT_CPP_DIR}\n"
                        "cd ${GODOT_CPP_DIR}\n"
                        "git checkout 4.3")
endif()

add_subdirectory(${GODOT_CPP_DIR})

# Source files
set(SOURCES
    src/register_types.cpp
    src/grpc_client.cpp
    src/grpc_channel_pool.cpp
    src/grpc_stream.cpp
    src/util/status_map.cpp
)

# Create the library
add_library(${LIBRARY_NAME} SHARED ${SOURCES})

# Include directories
target_include_directories(${LIBRARY_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(${LIBRARY_NAME} PRIVATE
    godot-cpp
    ${GRPC_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
)

# Compiler flags
if(MSVC)
    target_compile_options(${LIBRARY_NAME} PRIVATE /W4 /WX-)
    target_compile_definitions(${LIBRARY_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Use generator expressions for multi-config generators (Visual Studio)
    target_compile_options(${LIBRARY_NAME} PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Debug>:/Od>
    )
else()
    target_compile_options(${LIBRARY_NAME} PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
    # Optimization flags for Unix-like systems
    target_compile_options(${LIBRARY_NAME} PRIVATE
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Debug>:-Og>
    )
    # Strip symbols on Unix-like systems for Release builds
    if(APPLE)
        target_link_options(${LIBRARY_NAME} PRIVATE $<$<CONFIG:Release>:-Wl,-dead_strip>)
    elseif(UNIX)
        target_link_options(${LIBRARY_NAME} PRIVATE $<$<CONFIG:Release>:-Wl,--strip-all>)
    endif()
endif()

# Output configuration
set_target_properties(${LIBRARY_NAME} PROPERTIES
    OUTPUT_NAME ${OUTPUT_NAME}
    PREFIX "lib"
    SUFFIX ""
)

# Set output directory
set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/demo/addons/godot_grpc/bin/${GODOT_PLATFORM}")

# For multi-config generators, we need to set per-config output directories
if(CMAKE_CONFIGURATION_TYPES)
    foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${CONFIG} CONFIG_UPPER)
        set_target_properties(${LIBRARY_NAME} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${OUTPUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${OUTPUT_DIR}"
        )
    endforeach()
else()
    set_target_properties(${LIBRARY_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
    )
endif()

# Platform-specific settings
if(APPLE)
    set_target_properties(${LIBRARY_NAME} PROPERTIES
        SUFFIX ".dylib"
    )
elseif(UNIX)
    set_target_properties(${LIBRARY_NAME} PROPERTIES
        SUFFIX ".so"
    )
elseif(WIN32)
    set_target_properties(${LIBRARY_NAME} PROPERTIES
        SUFFIX ".dll"
    )
endif()

# Installation
install(TARGETS ${LIBRARY_NAME}
    LIBRARY DESTINATION addons/godot_grpc/bin/${GODOT_PLATFORM}
    RUNTIME DESTINATION addons/godot_grpc/bin/${GODOT_PLATFORM}
)

install(FILES godot/godot_grpc.gdextension
    DESTINATION addons/godot_grpc
)

# Print configuration
message(STATUS "=== godot_grpc Configuration ===")
message(STATUS "Platform: ${GODOT_PLATFORM}")
message(STATUS "Architecture: ${GODOT_ARCH}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Output Name: ${OUTPUT_NAME}")
message(STATUS "Output Directory: ${OUTPUT_DIR}")
message(STATUS "================================")
