cmake_minimum_required(VERSION 3.10)
project(dcv_smoke_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# The SDK that ships with this repository.
set(DCV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../dcv")

if(WIN32)
    set(DCV_LIB_DIR "${DCV_ROOT}/lib/win")
elseif(APPLE)
    set(DCV_LIB_DIR "${DCV_ROOT}/lib/mac")
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(DCV_LIB_DIR "${DCV_ROOT}/lib/linux/arm64")
    else()
        set(DCV_LIB_DIR "${DCV_ROOT}/lib/linux/x64")
    endif()
endif()

message(STATUS "Using Dynamsoft libraries from ${DCV_LIB_DIR}")

# Load the shared libraries from the executable directory so `ctest` works from
# the build tree on Linux and macOS without touching LD_LIBRARY_PATH.
if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-rpath,@loader_path")
elseif(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-rpath,$ORIGIN")
endif()

include_directories("${DCV_ROOT}/include")
link_directories("${DCV_LIB_DIR}")

add_executable(smoke_test smoke_test.cpp)

if(WIN32)
    target_link_libraries(smoke_test
        "DynamsoftCorex64"
        "DynamsoftLicensex64"
        "DynamsoftCaptureVisionRouterx64"
        "DynamsoftUtilityx64"
        "DynamsoftBarcodeReaderx64")
else()
    target_link_libraries(smoke_test
        "DynamsoftCore"
        "DynamsoftLicense"
        "DynamsoftCaptureVisionRouter"
        "DynamsoftUtility"
        "DynamsoftBarcodeReader"
        pthread)
endif()

# Everything the SDK loads at runtime must sit next to the executable.
add_custom_command(TARGET smoke_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${DCV_LIB_DIR}"
    $<TARGET_FILE_DIR:smoke_test>)

foreach(resource Templates Models ParserResources)
    add_custom_command(TARGET smoke_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DCV_ROOT}/resource/${resource}"
        $<TARGET_FILE_DIR:smoke_test>/${resource})
endforeach()

foreach(resource ConfusableChars.data OverlappingChars.data)
    if(EXISTS "${DCV_ROOT}/resource/${resource}")
        add_custom_command(TARGET smoke_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DCV_ROOT}/resource/${resource}"
            $<TARGET_FILE_DIR:smoke_test>/${resource})
    endif()
endforeach()

enable_testing()

set(TEST_IMAGES "${CMAKE_CURRENT_SOURCE_DIR}/../images")
set(TEST_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../license-key.txt")
set(TEST_PDF "${CMAKE_CURRENT_SOURCE_DIR}/../examples/command-line/multi_page.pdf")

# AllSupportedBarcodeTypes.png holds 18 barcodes across most 1D/2D symbologies;
# the threshold leaves room for minor decoder differences between platforms.
add_test(NAME barcode_image
    COMMAND smoke_test --mode barcode --min 15 --license-file "${TEST_LICENSE}"
            "${TEST_IMAGES}/AllSupportedBarcodeTypes.png")

add_test(NAME barcode_multipage_pdf
    COMMAND smoke_test --mode pages --min 4 --license-file "${TEST_LICENSE}"
            "${TEST_PDF}")

add_test(NAME mrz_passport
    COMMAND smoke_test --mode mrz --min 1 --license-file "${TEST_LICENSE}"
            "${TEST_IMAGES}/passport-sample.jpg")

set_tests_properties(barcode_image barcode_multipage_pdf mrz_passport
    PROPERTIES TIMEOUT 300)
