cmake_minimum_required(VERSION 3.8)
project(benchmark)
MESSAGE(STATUS "PROJECT_NAME: " ${PROJECT_NAME})

if (CMAKE_HOST_WIN32)
    set(WINDOWS 1)
elseif(CMAKE_HOST_APPLE)
    set(mac 1)
elseif(CMAKE_HOST_UNIX)
    set(LINUX 1)
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WINDOWS)
    MESSAGE(STATUS "Platform: Windows")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        MESSAGE(STATUS "Compiler: MSVC")
    endif()
endif()

# Set RPATH for Unix
if(CMAKE_HOST_UNIX)
    if(CMAKE_HOST_APPLE)
        SET(CMAKE_CXX_FLAGS "-std=c++17 -O3 -Wl,-rpath,@loader_path")
        SET(CMAKE_INSTALL_RPATH "@loader_path")
    else()
        SET(CMAKE_CXX_FLAGS "-std=c++17 -O3 -Wl,-rpath=$ORIGIN")
        SET(CMAKE_INSTALL_RPATH "$ORIGIN")
    endif()
    SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

# ============================================================
# Dynamsoft Capture Vision (relative paths)
# ============================================================
MESSAGE(STATUS "CPU architecture: ${CMAKE_SYSTEM_PROCESSOR}")

if(WINDOWS)
    link_directories("${PROJECT_SOURCE_DIR}/../../dcv/lib/win")
elseif(LINUX)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64)
        link_directories("${PROJECT_SOURCE_DIR}/../../dcv/lib/linux/x64")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
        link_directories("${PROJECT_SOURCE_DIR}/../../dcv/lib/linux/arm64/")
    endif()
elseif(mac)
    link_directories("${PROJECT_SOURCE_DIR}/../../dcv/lib/mac/")
endif()

include_directories("${PROJECT_SOURCE_DIR}/../../dcv/include/")

# ============================================================
# zxing-cpp (headers and lib are local)
# ============================================================
include_directories("${PROJECT_SOURCE_DIR}/zxing-cpp/include")
link_directories("${PROJECT_SOURCE_DIR}/lib/zxing-cpp")

# ============================================================
# stb (header-only image loading library)
# ============================================================
include_directories("${PROJECT_SOURCE_DIR}/stb")

# ============================================================
# Build executable
# ============================================================
add_executable(${PROJECT_NAME} main.cpp)

if(WINDOWS)
    if(CMAKE_CL_64)
        target_link_libraries(${PROJECT_NAME}
            "DynamsoftCorex64"
            "DynamsoftLicensex64"
            "DynamsoftCaptureVisionRouterx64"
            "DynamsoftUtilityx64"
            "DynamsoftBarcodeReaderx64"
            "ZXing"
        )
    endif()
else()
    target_link_libraries(${PROJECT_NAME}
        "DynamsoftCore"
        "DynamsoftLicense"
        "DynamsoftCaptureVisionRouter"
        "DynamsoftUtility"
        "DynamsoftBarcodeReader"
        "ZXing"
        pthread
    )
endif()

# ============================================================
# Post-build: copy Dynamsoft DLLs to output directory
# ============================================================
if(WINDOWS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/../../dcv/lib/win"
        $<TARGET_FILE_DIR:benchmark>)
elseif(LINUX)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/../../dcv/lib/linux/x64"
            $<TARGET_FILE_DIR:benchmark>)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/../../dcv/lib/linux/arm64"
            $<TARGET_FILE_DIR:benchmark>)
    endif()
elseif(mac)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/../../dcv/lib/mac/"
        $<TARGET_FILE_DIR:benchmark>)
endif()

# Copy Templates
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:benchmark>/Templates
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROJECT_SOURCE_DIR}/../../dcv/resource/Templates"
    $<TARGET_FILE_DIR:benchmark>/Templates)

# Copy Models
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:benchmark>/Models
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROJECT_SOURCE_DIR}/../../dcv/resource/Models"
    $<TARGET_FILE_DIR:benchmark>/Models)
