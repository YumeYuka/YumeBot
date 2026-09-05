cmake_minimum_required(VERSION 3.28)

# CMake 实验性 `import std` 支持（Clang + libc++ / libstdc++）
# 必须在 project() 之前设置
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
        "f35a9ac6-8463-4d38-8eec-5d6008153e7d"
)

project(YumeMeta
        VERSION 1.0.0
        LANGUAGES C CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(SetupCurl)
yumemeta_setup_curl()

set(CMAKE_CXX_SCAN_FOR_MODULES ON)

file(GLOB_RECURSE YUMEMETA_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp"
)

file(GLOB_RECURSE YUMEMETA_MODULES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.ixx"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.ixx"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cppm"
)

file(GLOB_RECURSE YUMEMETA_HEADERS
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/**/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/**/*.h"
)

if(YUMEMETA_SOURCES STREQUAL "" AND YUMEMETA_MODULES STREQUAL "")
    message(FATAL_ERROR "src/ 目录下没有找到 C++ 源文件或模块文件")
endif()

add_executable(YumeMeta)

# 启用 `import std;`（由 CMake 根据 libc++.modules.json 提供 std 模块）
set_property(TARGET YumeMeta PROPERTY CXX_MODULE_STD ON)

target_sources(YumeMeta
        PRIVATE
        ${YUMEMETA_SOURCES}
        ${YUMEMETA_HEADERS}
)

target_sources(YumeMeta
        PRIVATE
        FILE_SET CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES ${YUMEMETA_MODULES}
)

target_include_directories(YumeMeta
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(YumeMeta PRIVATE CURL::libcurl)

file(GLOB _module_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/src/*")
foreach(_dir IN LISTS _module_dirs)
    if(IS_DIRECTORY "${_dir}")
        target_include_directories(YumeMeta PRIVATE "${_dir}")
    endif()
endforeach()

message(STATUS "========================================")
message(STATUS "Project      : ${PROJECT_NAME}")
message(STATUS "C++ Standard : C++${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler     : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Stdlib       : ${CMAKE_CXX_STANDARD_LIBRARY}")
message(STATUS "import std   : ${CMAKE_CXX_COMPILER_IMPORT_STD}")
message(STATUS "Generator    : ${CMAKE_GENERATOR}")
message(STATUS "Sources      : ${YUMEMETA_SOURCES}")
message(STATUS "Modules      : ${YUMEMETA_MODULES}")
message(STATUS "========================================")
