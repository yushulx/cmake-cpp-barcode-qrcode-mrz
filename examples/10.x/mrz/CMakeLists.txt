cmake_minimum_required (VERSION 3.8)
project (main)
MESSAGE( STATUS "PROJECT_NAME: " ${PROJECT_NAME} )

option(ENABLE_OPENCV "Build with OpenCV" OFF)
MESSAGE(STATUS "Build with OpenCV: ${ENABLE_OPENCV}")

if (CMAKE_HOST_WIN32)
    set(WINDOWS 1)
elseif(CMAKE_HOST_APPLE)
    set(MACOS 1)
elseif(CMAKE_HOST_UNIX)
    set(LINUX 1)
endif()

# Check compiler architecture
if(CMAKE_CL_64)
    MESSAGE( STATUS ">>>>>>>> 64-bit")
else()
    MESSAGE( STATUS ">>>>>>>> 32-bit")
endif()

# Check compilers
MESSAGE( STATUS ">>>>>>>> ${CMAKE_CXX_COMPILER_ID}")
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    MESSAGE( STATUS "Using Clang" )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    MESSAGE( STATUS "Using GNU" )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    MESSAGE( STATUS "Using Intel" )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    MESSAGE( STATUS "Using MSVC" )
endif()

# Set RPATH
if(CMAKE_HOST_UNIX)
    if(CMAKE_HOST_APPLE)
        SET(CMAKE_CXX_FLAGS "-std=c++11 -O3 -Wl,-rpath,@loader_path")
        SET(CMAKE_INSTALL_RPATH "@loader_path")
    else()
        SET(CMAKE_CXX_FLAGS "-std=c++11 -O3 -Wl,-rpath=$ORIGIN")
        SET(CMAKE_INSTALL_RPATH "$ORIGIN")
    endif()
    SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

# Add search path for include and lib files
MESSAGE( STATUS "CPU architecture ${CMAKE_SYSTEM_PROCESSOR}" )
if(WINDOWS)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        link_directories("${PROJECT_SOURCE_DIR}/../sdk/platforms/win/bin/") 
    else()
        link_directories("${PROJECT_SOURCE_DIR}/../sdk/platforms/win/lib/") 
    endif()
elseif(LINUX)
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64)
        MESSAGE( STATUS "Link directory: ${PROJECT_SOURCE_DIR}/../sdk/platforms/linux/" )
        link_directories("${PROJECT_SOURCE_DIR}/../sdk/platforms/linux/")
    endif()
elseif(MACOS)
    link_directories("${PROJECT_SOURCE_DIR}/../sdk/platforms/macos/")
endif()
include_directories("${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/../sdk/include/")

# Add the executable
if (ENABLE_OPENCV)
    find_package(OpenCV REQUIRED)
    add_executable(${PROJECT_NAME} maincv.cpp)
    if(WINDOWS)
        if(CMAKE_CL_64)
            target_link_libraries (${PROJECT_NAME} "DynamsoftCorex64" "DynamsoftLicensex64" "DynamsoftCaptureVisionRouterx64" "DynamsoftUtilityx64" ${OpenCV_LIBS})
        endif()
    else()
        target_link_libraries (${PROJECT_NAME} "DynamsoftCore" "DynamsoftLicense" "DynamsoftCaptureVisionRouter" "DynamsoftUtility" pthread ${OpenCV_LIBS})
    endif()
else()
    add_executable(${PROJECT_NAME} main.cpp)
    if(WINDOWS)
        if(CMAKE_CL_64)
            target_link_libraries (${PROJECT_NAME} "DynamsoftCorex64" "DynamsoftLicensex64" "DynamsoftCaptureVisionRouterx64" "DynamsoftUtilityx64" )
        endif()
    else()
        target_link_libraries (${PROJECT_NAME} "DynamsoftCore" "DynamsoftLicense" "DynamsoftCaptureVisionRouter" "DynamsoftUtility"  pthread)
    endif()
endif()

if(WINDOWS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROJECT_SOURCE_DIR}/../sdk/platforms/win/bin/"      
    $<TARGET_FILE_DIR:main>)
elseif(LINUX)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROJECT_SOURCE_DIR}/../sdk/platforms/linux/"      
    $<TARGET_FILE_DIR:main>)
elseif(MACOS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROJECT_SOURCE_DIR}/../sdk/platforms/macos/"      
    $<TARGET_FILE_DIR:main>)
endif()

# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
# COMMAND ${CMAKE_COMMAND} -E copy
# "${PROJECT_SOURCE_DIR}/../sdk/Templates/DBR-PresetTemplates.json"
# $<TARGET_FILE_DIR:main>/DBR-PresetTemplates.json)

# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
# COMMAND ${CMAKE_COMMAND} -E copy
# "${PROJECT_SOURCE_DIR}/../sdk/Templates/DLR-PresetTemplates.json"
# $<TARGET_FILE_DIR:main>/DLR-PresetTemplates.json)

# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
# COMMAND ${CMAKE_COMMAND} -E copy
# "${PROJECT_SOURCE_DIR}/../sdk/Templates/MRZScanner.json"
# $<TARGET_FILE_DIR:main>/MRZScanner.json)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy
"${PROJECT_SOURCE_DIR}/../sdk/ConfusableChars.data"
$<TARGET_FILE_DIR:main>/ConfusableChars.data)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy
"${PROJECT_SOURCE_DIR}/../sdk/OverlappingChars.data"
$<TARGET_FILE_DIR:main>/OverlappingChars.data)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:main>/Models
COMMAND ${CMAKE_COMMAND} -E copy_directory
"${PROJECT_SOURCE_DIR}/../sdk/Models"
$<TARGET_FILE_DIR:main>/Models)


add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:main>/ParserResources
COMMAND ${CMAKE_COMMAND} -E copy_directory
"${PROJECT_SOURCE_DIR}/../sdk/ParserResources"
$<TARGET_FILE_DIR:main>/ParserResources)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:main>/Templates
COMMAND ${CMAKE_COMMAND} -E copy_directory
"${PROJECT_SOURCE_DIR}/../sdk/Templates"
$<TARGET_FILE_DIR:main>/Templates)