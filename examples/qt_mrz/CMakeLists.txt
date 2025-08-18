cmake_minimum_required(VERSION 3.5)

project(MRZRecognizer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_HOST_WIN32)
    link_directories("${PROJECT_SOURCE_DIR}/platform/windows/lib/") 
elseif(CMAKE_HOST_UNIX)
    link_directories("${PROJECT_SOURCE_DIR}/platform/linux/")
endif()
include_directories("${PROJECT_SOURCE_DIR}/include/")

# Find Qt5 components - specify Qt5_DIR for MSVC builds on Windows
# Example: cmake -DQt5_DIR="C:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5" ..
find_package(Qt5 COMPONENTS Widgets Multimedia MultimediaWidgets REQUIRED)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        myvideosurface.h
        myvideosurface.cpp
        work.h
        work.cpp
        mrzinfo.h
        mrzinfo.cpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

if (CMAKE_HOST_WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Widgets Qt5::Multimedia Qt5::MultimediaWidgets "DynamsoftLabelRecognizerx64")    
elseif(CMAKE_HOST_UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Widgets Qt5::Multimedia Qt5::MultimediaWidgets "DynamsoftLabelRecognizer")
endif()

# Copy DLLs
if(CMAKE_HOST_WIN32)
    # Copy Dynamsoft DLLs
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/platform/windows/bin/"      
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    
    # Automatically copy Qt DLLs for MSVC builds
    if(MSVC)
        # Get Qt5 installation path
        get_target_property(QT5_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
        get_filename_component(QT5_WINDEPLOYQT_EXECUTABLE ${QT5_QMAKE_EXECUTABLE} PATH)
        set(QT5_WINDEPLOYQT_EXECUTABLE "${QT5_WINDEPLOYQT_EXECUTABLE}/windeployqt.exe")
        
        # Use windeployqt to copy Qt DLLs and plugins automatically
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${QT5_WINDEPLOYQT_EXECUTABLE} --qmldir ${CMAKE_SOURCE_DIR} $<TARGET_FILE:${PROJECT_NAME}>
            COMMENT "Deploying Qt libraries")
            
        # Alternative manual copying if windeployqt is not available
        # Copy essential Qt DLLs
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Core> $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Gui> $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Widgets> $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Multimedia> $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::MultimediaWidgets> $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Network> $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "Copying Qt DLLs")
            
        # Copy Qt platform plugins
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms
            COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                "${Qt5_DIR}/../../../plugins/platforms/qwindows.dll"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms/
            COMMENT "Copying Qt platform plugins")
            
        # Copy Qt multimedia plugins
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/mediaservice
            COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                "${Qt5_DIR}/../../../plugins/mediaservice/qtmedia_audioengine.dll"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/mediaservice/
            COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                "${Qt5_DIR}/../../../plugins/mediaservice/dsengine.dll"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/mediaservice/
            COMMENT "Copying Qt multimedia plugins")
    endif()
endif()

# Copy template
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/template/"      
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)

# Copy model files
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/model"      
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/model)
