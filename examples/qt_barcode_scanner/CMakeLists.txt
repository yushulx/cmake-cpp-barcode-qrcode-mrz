cmake_minimum_required(VERSION 3.16)

project(QtBarcodeScanner VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set Qt directory manually
set(Qt6_DIR "C:/Qt/6.7.2/msvc2019_64/lib/cmake/Qt6")
set(CMAKE_PREFIX_PATH "C:/Qt/6.7.2/msvc2019_64")

# Enable Qt's MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Platform detection
if(WIN32)
    set(WINDOWS TRUE)
elseif(UNIX)
    if(APPLE)
        set(MACOS TRUE)
    else()
        set(LINUX TRUE)
    endif()
endif()

# Define camera support - OpenCV only
add_definitions(-DENABLE_OPENCV_CAMERA=1)

# Include DCR headers
include_directories("${PROJECT_SOURCE_DIR}/../../dcv/include/")

# Include OpenCV headers
include_directories("${PROJECT_SOURCE_DIR}/../../cvcamera/include/")

# Find Qt6 - Core, Widgets, and Multimedia components
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia MultimediaWidgets)

# Simple approach - define source files directly
set(SOURCES
    main.cpp
    mainwindow.cpp
    barcodeworker.cpp
    opencvcamera.cpp
    mainwindow.ui
)

set(HEADERS
    mainwindow.h
    barcodeworker.h
    opencvcamera.h
)

# Add executable
add_executable(QtBarcodeScanner WIN32 ${SOURCES} ${HEADERS})

# Link Qt libraries - Core, Widgets, and Multimedia
target_link_libraries(QtBarcodeScanner PRIVATE 
    Qt6::Core 
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

# DCR library paths and linking
if(WINDOWS)
    # Set library directories - use absolute path
    set(DCR_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../dcv/lib/win")
    set(OPENCV_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../cvcamera/lib/windows")
    
    # Link DCR libraries with full paths
    target_link_libraries(QtBarcodeScanner PRIVATE 
        "${DCR_LIB_DIR}/DynamsoftCaptureVisionRouterx64.lib"
        "${DCR_LIB_DIR}/DynamsoftLicensex64.lib"
        "${DCR_LIB_DIR}/DynamsoftCorex64.lib"
        "${DCR_LIB_DIR}/DynamsoftUtilityx64.lib"
        "${DCR_LIB_DIR}/DynamsoftBarcodeReaderx64.lib"
        # OpenCV libraries for hybrid camera support
        "${OPENCV_LIB_DIR}/opencv_core480.lib"
        "${OPENCV_LIB_DIR}/opencv_imgproc480.lib"
        "${OPENCV_LIB_DIR}/opencv_highgui480.lib"
        "${OPENCV_LIB_DIR}/opencv_videoio480.lib"
        "${OPENCV_LIB_DIR}/opencv_imgcodecs480.lib"
    )
elseif(MACOS)
    link_directories("${PROJECT_SOURCE_DIR}/../../dcv/lib/mac/")
    target_link_libraries(QtBarcodeScanner PRIVATE 
        "DynamsoftCaptureVisionRouter"
        "DynamsoftLicense"
        "DynamsoftCore"
        "DynamsoftUtility"
        "DynamsoftBarcodeReader"
    )
elseif(LINUX)
    link_directories("${PROJECT_SOURCE_DIR}/../../dcv/lib/linux/")
    target_link_libraries(QtBarcodeScanner PRIVATE 
        "DynamsoftCaptureVisionRouter"
        "DynamsoftLicense"
        "DynamsoftCore"
        "DynamsoftUtility" 
        "DynamsoftBarcodeReader"
    )
endif()

# Copy DLLs for Windows
if(WINDOWS)
    # Copy Qt DLLs
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/bin/Qt6Core.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/bin/Qt6Gui.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/bin/Qt6Widgets.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/bin/Qt6Multimedia.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/bin/Qt6MultimediaWidgets.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/bin/Qt6Network.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>)
    
    # Create imageformats directory and copy Qt image format plugins
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:QtBarcodeScanner>/imageformats)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/plugins/imageformats/qjpeg.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>/imageformats/)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/plugins/imageformats/qgif.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>/imageformats/)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/plugins/imageformats/qtiff.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>/imageformats/)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/plugins/imageformats/qwebp.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>/imageformats/)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/plugins/imageformats/qico.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>/imageformats/)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/plugins/imageformats/qsvg.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>/imageformats/)
    
    # Create platforms directory and copy Qt platform plugins
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:QtBarcodeScanner>/platforms)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/plugins/platforms/qwindows.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>/platforms/)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/plugins/platforms/qminimal.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>/platforms/)
    
    # Copy Dynamsoft resource files (Templates and Models)
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory 
        $<TARGET_FILE_DIR:QtBarcodeScanner>/Templates
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/../../dcv/resource/Templates"
        $<TARGET_FILE_DIR:QtBarcodeScanner>/Templates)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory 
        $<TARGET_FILE_DIR:QtBarcodeScanner>/Models
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/../../dcv/resource/Models"
        $<TARGET_FILE_DIR:QtBarcodeScanner>/Models)
    
    # Copy DCR DLLs - Copy entire directory like command-line example
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/../../dcv/lib/win"      
        $<TARGET_FILE_DIR:QtBarcodeScanner>)
    
    # Copy OpenCV DLLs for hybrid camera support
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/../../cvcamera/lib/windows/opencv_core480.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/../../cvcamera/lib/windows/opencv_imgproc480.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/../../cvcamera/lib/windows/opencv_highgui480.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/../../cvcamera/lib/windows/opencv_videoio480.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>)
    
    add_custom_command(TARGET QtBarcodeScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/../../cvcamera/lib/windows/opencv_imgcodecs480.dll"
        $<TARGET_FILE_DIR:QtBarcodeScanner>)
endif()

# Set output directories
set_target_properties(QtBarcodeScanner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
