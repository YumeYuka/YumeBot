cmake_minimum_required(VERSION 4.3)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
        "f35a9ac6-8463-4d38-8eec-5d6008153e7d"
)

# Linux CI/images often lack libstdc++.modules.json; LLVM libc++ ships the metadata.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(_yumebot_clangxx NAMES clang++ clang++-23 clang++-22)
    if(_yumebot_clangxx)
        execute_process(
                COMMAND "${_yumebot_clangxx}" -dumpversion
                OUTPUT_VARIABLE _yumebot_clang_version
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
        )
        string(REGEX MATCH "^([0-9]+)" _yumebot_clang_major "${_yumebot_clang_version}")
        if(_yumebot_clang_major)
            set(_yumebot_libcxx_modules_json
                    "/usr/lib/llvm-${_yumebot_clang_major}/lib/libc++.modules.json")
            if(EXISTS "${_yumebot_libcxx_modules_json}")
                set(CMAKE_CXX_STDLIB_MODULES_JSON
                        "${_yumebot_libcxx_modules_json}" CACHE FILEPATH "" FORCE)
                set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
                set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
                message(STATUS "Using libc++ modules metadata: ${_yumebot_libcxx_modules_json}")
            endif()
        endif()
    endif()
endif()

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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_link_options(YumeBot PRIVATE -stdlib=libc++ -lc++abi "-Wl,-rpath,\$ORIGIN/lib")
    set_target_properties(YumeBot PROPERTIES
            BUILD_RPATH "$ORIGIN/lib"
            INSTALL_RPATH "$ORIGIN/lib"
    )
endif()

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
