cmake_minimum_required(VERSION 3.15)
project(MRZScanner)

if(WIN32)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded") 
    endif()
    
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../dist/lib/windows/release ${CMAKE_CURRENT_SOURCE_DIR}/../../../dcv/lib/win)
    else()
        link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../dist/lib/windows/debug ${CMAKE_CURRENT_SOURCE_DIR}/../../../dcv/lib/win)
    endif()
    
    set(DBR_LIBS "DynamsoftCorex64" "DynamsoftLicensex64" "DynamsoftCaptureVisionRouterx64" "DynamsoftUtilityx64")

elseif(APPLE)
    set(CMAKE_CXX_FLAGS "-std=c++11 -O3 -Wl,-rpath,@executable_path")
    set(CMAKE_INSTALL_RPATH "@executable_path")

    link_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dist/lib/macos
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../dcv/lib/mac
    )

    set(DBR_LIBS
        "DynamsoftCore"
        "DynamsoftLicense"
        "DynamsoftCaptureVisionRouter"
        "DynamsoftUtility"
        "pthread"
    )
elseif(UNIX)
    SET(CMAKE_CXX_FLAGS "-std=c++11 -O3 -Wl,-rpath=$ORIGIN")
    SET(CMAKE_INSTALL_RPATH "$ORIGIN")
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../dist/lib/linux ${CMAKE_CURRENT_SOURCE_DIR}/../../../dcv/lib/linux)
    set(DBR_LIBS "DynamsoftCore" "DynamsoftLicense" "DynamsoftCaptureVisionRouter" "DynamsoftUtility" pthread)
endif()

# Create the executable
add_executable(${PROJECT_NAME} main.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../dist/include ${CMAKE_CURRENT_SOURCE_DIR}/../../../dcv/include)
target_link_libraries(${PROJECT_NAME} litecam ${DBR_LIBS})

if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dist/lib/windows/release   
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    else()
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dist/lib/windows/debug   
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    endif()

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../dcv/lib/win     
    $<TARGET_FILE_DIR:${PROJECT_NAME}>)
elseif(APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../dcv/lib/mac
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
elseif(UNIX)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../dcv/lib/linux/      
    $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()


add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/Templates
COMMAND ${CMAKE_COMMAND} -E copy_directory
${CMAKE_CURRENT_SOURCE_DIR}/../../../dcv/resource/Templates
$<TARGET_FILE_DIR:${PROJECT_NAME}>/Templates)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/Models
COMMAND ${CMAKE_COMMAND} -E copy_directory
${CMAKE_CURRENT_SOURCE_DIR}/../../../dcv/resource/Models
$<TARGET_FILE_DIR:${PROJECT_NAME}>/Models)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/ParserResources
COMMAND ${CMAKE_COMMAND} -E copy_directory
${CMAKE_CURRENT_SOURCE_DIR}/../../../dcv/resource/ParserResources
$<TARGET_FILE_DIR:${PROJECT_NAME}>/ParserResources)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy
${CMAKE_CURRENT_SOURCE_DIR}/../../../dcv/resource/ConfusableChars.data
$<TARGET_FILE_DIR:${PROJECT_NAME}>/ConfusableChars.data)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy
${CMAKE_CURRENT_SOURCE_DIR}/../../../dcv/resource/OverlappingChars.data
$<TARGET_FILE_DIR:${PROJECT_NAME}>/OverlappingChars.data)
