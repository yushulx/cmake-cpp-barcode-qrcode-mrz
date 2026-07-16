cmake_minimum_required(VERSION 3.16)
project(barcode_benchmark VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(CTest)

# Build the checked-out ZXing-C++ submodule as part of this project.
set(ZXING_READERS ON CACHE BOOL "" FORCE)
set(ZXING_WRITERS OFF CACHE STRING "" FORCE)
set(ZXING_C_API OFF CACHE BOOL "" FORCE)
set(ZXING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZXING_EXAMPLES_QT OFF CACHE BOOL "" FORCE)
set(ZXING_BLACKBOX_TESTS OFF CACHE BOOL "" FORCE)
set(ZXING_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(ZXING_PYTHON_MODULE OFF CACHE BOOL "" FORCE)
set(ZXING_DISABLE_PCH ON CACHE BOOL "Avoid MSVC multi-config PCH races" FORCE)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/zxing-cpp/CMakeLists.txt")
    message(FATAL_ERROR "ZXing-C++ submodule is missing. Run: git submodule update --init --recursive")
endif()
add_subdirectory(zxing-cpp EXCLUDE_FROM_ALL)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

set(DCV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../dcv" CACHE PATH "Dynamsoft Capture Vision SDK root")

add_library(benchmark_core
    src/barber_dataset.cpp
    src/hash.cpp
    src/image_loader.cpp
    src/matcher.cpp
    src/metrics.cpp
    src/normalization.cpp
    src/result_writer.cpp
)
target_include_directories(benchmark_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/stb"
)
target_link_libraries(benchmark_core PUBLIC nlohmann_json::nlohmann_json)

add_library(benchmark_decoders
    src/dynamsoft_decoder.cpp
    src/zxing_decoder.cpp
)
target_include_directories(benchmark_decoders PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${DCV_ROOT}/include"
)
target_link_libraries(benchmark_decoders PUBLIC benchmark_core ZXing::Core)
target_compile_definitions(benchmark_decoders PUBLIC NOMINMAX)

if(WIN32)
    target_link_directories(benchmark_decoders PUBLIC "${DCV_ROOT}/lib/win")
    target_link_libraries(benchmark_decoders PUBLIC
        DynamsoftCorex64 DynamsoftLicensex64 DynamsoftCaptureVisionRouterx64
        DynamsoftUtilityx64 DynamsoftBarcodeReaderx64)
elseif(APPLE)
    target_link_directories(benchmark_decoders PUBLIC "${DCV_ROOT}/lib/mac")
    target_link_libraries(benchmark_decoders PUBLIC
        DynamsoftCore DynamsoftLicense DynamsoftCaptureVisionRouter
        DynamsoftUtility DynamsoftBarcodeReader)
    set(CMAKE_BUILD_RPATH "@loader_path")
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(DCV_LIB_DIR "${DCV_ROOT}/lib/linux/arm64")
    else()
        set(DCV_LIB_DIR "${DCV_ROOT}/lib/linux/x64")
    endif()
    target_link_directories(benchmark_decoders PUBLIC "${DCV_LIB_DIR}")
    target_link_libraries(benchmark_decoders PUBLIC
        DynamsoftCore DynamsoftLicense DynamsoftCaptureVisionRouter
        DynamsoftUtility DynamsoftBarcodeReader pthread)
    set(CMAKE_BUILD_RPATH "$ORIGIN")
endif()

add_executable(barcode_benchmark src/main.cpp)
target_link_libraries(barcode_benchmark PRIVATE benchmark_core benchmark_decoders)

if(WIN32)
    add_custom_command(TARGET barcode_benchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DCV_ROOT}/lib/win" "$<TARGET_FILE_DIR:barcode_benchmark>")
elseif(APPLE)
    add_custom_command(TARGET barcode_benchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DCV_ROOT}/lib/mac" "$<TARGET_FILE_DIR:barcode_benchmark>")
else()
    add_custom_command(TARGET barcode_benchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DCV_LIB_DIR}" "$<TARGET_FILE_DIR:barcode_benchmark>")
endif()

add_custom_command(TARGET barcode_benchmark POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:barcode_benchmark>/Templates"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${DCV_ROOT}/resource/Templates"
    "$<TARGET_FILE_DIR:barcode_benchmark>/Templates")
add_custom_command(TARGET barcode_benchmark POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:barcode_benchmark>/Models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${DCV_ROOT}/resource/Models"
    "$<TARGET_FILE_DIR:barcode_benchmark>/Models")

if(BUILD_TESTING)
    add_executable(benchmark_tests
        tests/test_main.cpp
        tests/test_barber_parser.cpp
        tests/test_matching.cpp
        tests/test_metrics.cpp
    )
    target_link_libraries(benchmark_tests PRIVATE benchmark_core)
    add_test(NAME benchmark_tests COMMAND benchmark_tests)
endif()
