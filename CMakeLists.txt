cmake_minimum_required(VERSION 4.3)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
        "f35a9ac6-8463-4d38-8eec-5d6008153e7d"
)

project(YumeBot
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
yumebot_setup_curl()

set(CMAKE_CXX_SCAN_FOR_MODULES ON)

file(GLOB_RECURSE YUMEBOT_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp"
)

file(GLOB_RECURSE YUMEBOT_MODULES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.ixx"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.ixx"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cppm"
)

file(GLOB_RECURSE YUMEBOT_HEADERS
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

if(YUMEBOT_SOURCES STREQUAL "" AND YUMEBOT_MODULES STREQUAL "")
    message(FATAL_ERROR "No C++ source files or module files were found in the src/ directory.")
endif()

add_executable(YumeBot)

set_property(TARGET YumeBot PROPERTY CXX_MODULE_STD ON)

target_sources(YumeBot
        PRIVATE
        ${YUMEBOT_SOURCES}
        ${YUMEBOT_HEADERS}
)

target_sources(YumeBot
        PRIVATE
        FILE_SET CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES ${YUMEBOT_MODULES}
)

target_include_directories(YumeBot
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(YumeBot PRIVATE CURL::libcurl)

file(GLOB _module_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/src/*")
foreach(_dir IN LISTS _module_dirs)
    if(IS_DIRECTORY "${_dir}")
        target_include_directories(YumeBot PRIVATE "${_dir}")
    endif()
endforeach()

install(TARGETS YumeBot RUNTIME DESTINATION bin)

message(\n)
message(STATUS "Project      : ${PROJECT_NAME}")
message(STATUS "C++ Standard : C++${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler     : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Stdlib       : ${CMAKE_CXX_STANDARD_LIBRARY}")
message(STATUS "import std   : ${CMAKE_CXX_COMPILER_IMPORT_STD}")
message(STATUS "Generator    : ${CMAKE_GENERATOR}")
message(\n)
