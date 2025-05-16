cmake_minimum_required(VERSION 3.10)
project(camera_example)

if(WIN32)
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../lib/windows ${CMAKE_CURRENT_SOURCE_DIR}/../../dcv/lib/win)
    file(GLOB DLL_FILES "${CMAKE_CURRENT_SOURCE_DIR}/../lib/windows/*.dll")
    set(DBR_LIBS "DynamsoftCorex64" "DynamsoftLicensex64" "DynamsoftCaptureVisionRouterx64" "DynamsoftUtilityx64")
elseif(UNIX)
    SET(CMAKE_CXX_FLAGS "-std=c++11 -O3 -Wl,-rpath=$ORIGIN")
    SET(CMAKE_INSTALL_RPATH "$ORIGIN")
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../lib/linux ${CMAKE_CURRENT_SOURCE_DIR}/../../dcv/lib/linux/x64)
    set(DBR_LIBS "DynamsoftCore" "DynamsoftLicense" "DynamsoftCaptureVisionRouter" "DynamsoftUtility" pthread)
endif()

# Create the executable
add_executable(camera_example main.cpp)
target_include_directories(camera_example PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/../../dcv/include)
target_link_libraries(camera_example camera_lib ${DBR_LIBS})

if(WIN32)
    add_custom_command(TARGET camera_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:camera_lib> $<TARGET_FILE_DIR:camera_example>)

    # Copy the DLL files to the executable directory
    foreach(DLL_FILE ${DLL_FILES})
        add_custom_command(TARGET camera_example POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DLL_FILE}
            $<TARGET_FILE_DIR:camera_example>)
    endforeach()

    add_custom_command(TARGET camera_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dcv/lib/win/      
    $<TARGET_FILE_DIR:camera_example>)
endif()

if(UNIX)
    add_custom_command(TARGET camera_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:camera_lib> $<TARGET_FILE_DIR:camera_example>)
endif()

add_custom_command(TARGET camera_example POST_BUILD
COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:camera_example>/Models
COMMAND ${CMAKE_COMMAND} -E copy_directory
${CMAKE_CURRENT_SOURCE_DIR}/../../dcv/resource/Models
$<TARGET_FILE_DIR:camera_example>/Models)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:camera_example>/Templates
COMMAND ${CMAKE_COMMAND} -E copy_directory
${CMAKE_CURRENT_SOURCE_DIR}/../../dcv/resource/Templates
$<TARGET_FILE_DIR:camera_example>/Templates)