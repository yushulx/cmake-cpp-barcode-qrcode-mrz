cmake_minimum_required (VERSION 3.8)
project (main)
MESSAGE( STATUS "PROJECT_NAME: " ${PROJECT_NAME} )

if (CMAKE_HOST_WIN32)
    set(WINDOWS 1)
elseif(CMAKE_HOST_APPLE)
    set(mac 1)
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
        link_directories("${PROJECT_SOURCE_DIR}/../../dcv/lib/win") 
    else()
        link_directories("${PROJECT_SOURCE_DIR}/../../dcv/lib/win") 
    endif()
elseif(LINUX)
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64)
        MESSAGE( STATUS "Link directory: ${PROJECT_SOURCE_DIR}/../../dcv/lib/linux/x64" )
        link_directories("${PROJECT_SOURCE_DIR}/../../dcv/lib/linux/x64")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64) 
        MESSAGE( STATUS "Link directory: ${PROJECT_SOURCE_DIR}/../../dcv/lib/linux/arm64/" )
        link_directories("${PROJECT_SOURCE_DIR}/../../dcv/lib/linux/arm64/") 
    endif()
elseif(mac)
    MESSAGE( STATUS "Link directory: ${PROJECT_SOURCE_DIR}/../../dcv/lib/mac/" )
    link_directories("${PROJECT_SOURCE_DIR}/../../dcv/lib/mac/") 
endif()
include_directories("${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/../../dcv/include/")

# Add the executable
add_executable(${PROJECT_NAME} main.cpp)
if(WINDOWS)
    if(CMAKE_CL_64)
        target_link_libraries (${PROJECT_NAME} "DynamsoftCorex64" "DynamsoftLicensex64" "DynamsoftCaptureVisionRouterx64" "DynamsoftUtilityx64" )
    endif()
else()
    target_link_libraries (${PROJECT_NAME} "DynamsoftCore" "DynamsoftLicense" "DynamsoftCaptureVisionRouter" "DynamsoftUtility"  pthread)
endif()

if(WINDOWS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROJECT_SOURCE_DIR}/../../dcv/lib/win"      
    $<TARGET_FILE_DIR:main>)
elseif(LINUX)
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/../../dcv/lib/linux/x64"      
        $<TARGET_FILE_DIR:main>)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64) 
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/../../dcv/lib/linux/arm64"      
        $<TARGET_FILE_DIR:main>)
    endif()
elseif(mac)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROJECT_SOURCE_DIR}/../../dcv/lib/mac/"      
    $<TARGET_FILE_DIR:main>)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:main>/Templates
COMMAND ${CMAKE_COMMAND} -E copy_directory
"${PROJECT_SOURCE_DIR}/../../dcv/resource/Templates"
$<TARGET_FILE_DIR:main>/Templates)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:main>/Models
COMMAND ${CMAKE_COMMAND} -E copy_directory
"${PROJECT_SOURCE_DIR}/../../dcv/resource/Models"
$<TARGET_FILE_DIR:main>/Models)