cmake_minimum_required (VERSION 3.8)
project(raspberry_pi_barcode_reader)
MESSAGE( STATUS "PROJECT_NAME: " ${PROJECT_NAME} )

# Set RPATH so the executable finds the Dynamsoft libraries copied next to it.
SET(CMAKE_CXX_FLAGS "-std=c++11 -O3 -Wl,-rpath=$ORIGIN")
SET(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# The Dynamsoft Capture Vision libraries are checked in at ../../dcv
set(DCV_ROOT "${PROJECT_SOURCE_DIR}/../../dcv")

# Select the Linux library directory for the current CPU architecture.
if(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64 OR CMAKE_SYSTEM_PROCESSOR STREQUAL arm64)
    MESSAGE( STATUS "Link directory: ${DCV_ROOT}/lib/linux/arm64/" )
    set(DCV_LIB_DIR "${DCV_ROOT}/lib/linux/arm64/")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64 OR CMAKE_SYSTEM_PROCESSOR STREQUAL amd64)
    MESSAGE( STATUS "Link directory: ${DCV_ROOT}/lib/linux/x64/" )
    set(DCV_LIB_DIR "${DCV_ROOT}/lib/linux/x64/")
else()
    MESSAGE( FATAL_ERROR "Unsupported CPU architecture: ${CMAKE_SYSTEM_PROCESSOR}" )
endif()
link_directories("${DCV_LIB_DIR}")

include_directories("${PROJECT_BINARY_DIR}" "${DCV_ROOT}/include/")

# Add the executable
add_executable(barcode_reader main.cxx v4l2_capture.cpp ssd1306_i2c.c)
target_link_libraries (barcode_reader "DynamsoftCore" "DynamsoftLicense"
                       "DynamsoftCaptureVisionRouter" "DynamsoftUtility"
                       "DynamsoftBarcodeReader" pthread dl)

# Copy the Dynamsoft libraries and the runtime resources next to the executable.
add_custom_command(TARGET barcode_reader POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DCV_LIB_DIR}"
        $<TARGET_FILE_DIR:barcode_reader>)

add_custom_command(TARGET barcode_reader POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:barcode_reader>/Models
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DCV_ROOT}/resource/Models"
        $<TARGET_FILE_DIR:barcode_reader>/Models)

add_custom_command(TARGET barcode_reader POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:barcode_reader>/Templates
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DCV_ROOT}/resource/Templates"
        $<TARGET_FILE_DIR:barcode_reader>/Templates)

# Installation
install (DIRECTORY "${DCV_LIB_DIR}" DESTINATION lib)
install (TARGETS barcode_reader DESTINATION bin)
