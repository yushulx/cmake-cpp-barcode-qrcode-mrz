cmake_minimum_required (VERSION 3.8)
project (BarcodeReader)
MESSAGE( STATUS "PROJECT_NAME: " ${PROJECT_NAME} )

option(ARM32_BUILD "Build for ARM32" OFF)

# Check platforms
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
        link_directories("${PROJECT_SOURCE_DIR}/platforms/win/bin/") 
    else()
        link_directories("${PROJECT_SOURCE_DIR}/platforms/win/lib/") 
    endif()
elseif(LINUX)
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64)
        MESSAGE( STATUS "Link directory: ${PROJECT_SOURCE_DIR}/platforms/linux/" )
        link_directories("${PROJECT_SOURCE_DIR}/platforms/linux/")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL armv7l OR ARM32_BUILD)
        MESSAGE( STATUS "Link directory: ${PROJECT_SOURCE_DIR}/platforms/arm32/" )
        link_directories("${PROJECT_SOURCE_DIR}/platforms/arm32/") 
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64) 
        MESSAGE( STATUS "Link directory: ${PROJECT_SOURCE_DIR}/platforms/aarch64/" )
        link_directories("${PROJECT_SOURCE_DIR}/platforms/aarch64/") 
    endif()
elseif(MACOS)
    MESSAGE( STATUS "Link directory: ${PROJECT_SOURCE_DIR}/platforms/macos/" )
    link_directories("${PROJECT_SOURCE_DIR}/platforms/macos/") 
endif()
include_directories("${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include/")

# Add bridge project
add_subdirectory(bridge)

# Add the executable
add_executable(${PROJECT_NAME} BarcodeReader.cxx)
if(WINDOWS)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries (${PROJECT_NAME}  "bridge")
    else()
        if(CMAKE_CL_64)
            target_link_libraries (${PROJECT_NAME} "bridge")
        endif()
    endif()
else()
    target_link_libraries (${PROJECT_NAME} "DynamsoftBarcodeReader" pthread)
endif()

if(WINDOWS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROJECT_SOURCE_DIR}/platforms/win/bin/"      
    $<TARGET_FILE_DIR:BarcodeReader>)
endif()

# Set installation directory
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/dist)
if(WINDOWS)
    install (DIRECTORY  "${PROJECT_SOURCE_DIR}/platforms/win/bin/" DESTINATION ${PROJECT_SOURCE_DIR}/dist)
elseif(LINUX)
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64)
        install (DIRECTORY  "${PROJECT_SOURCE_DIR}/platforms/linux/" DESTINATION ${PROJECT_SOURCE_DIR}/dist)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL armv7l OR ARM32_BUILD)
        install (DIRECTORY  "${PROJECT_SOURCE_DIR}/platforms/arm32/" DESTINATION ${PROJECT_SOURCE_DIR}/dist)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64) 
        install (DIRECTORY  "${PROJECT_SOURCE_DIR}/platforms/aarch64/" DESTINATION ${PROJECT_SOURCE_DIR}/dist)
    endif()
elseif(MACOS)
    install (DIRECTORY  "${PROJECT_SOURCE_DIR}/platforms/macos/" DESTINATION ${PROJECT_SOURCE_DIR}/dist)
endif()

# install (FILES "${PROJECT_BINARY_DIR}/BarcodeReaderConfig.h" DESTINATION include)
# install (DIRECTORY "${PROJECT_SOURCE_DIR}/include/" DESTINATION include)

# Use CTest
include(CTest)
add_test (BarcodeReaderRuns ${PROJECT_NAME})
