cmake_minimum_required(VERSION 3.0.0)

project(bridge VERSION 0.1.0)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    link_directories("${PROJECT_SOURCE_DIR}/../platforms/win/bin/") 
else()
    link_directories("${PROJECT_SOURCE_DIR}/../platforms/win/lib/") 
endif()

INCLUDE_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/../include/")
add_library(bridge SHARED bridge.cpp)
target_compile_definitions(bridge PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries (${PROJECT_NAME} "DynamsoftCorex64" "DynamsoftLicensex64" "DynamsoftCaptureVisionRouterx64" "DynamsoftUtilityx64" "DynamsoftBarcodeReaderx64")

if(WIN32)
    add_custom_command(TARGET bridge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/platforms/win/bin/"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/platforms/win/lib/"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:bridge> "${CMAKE_SOURCE_DIR}/platforms/win/bin/"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_LINKER_FILE:bridge> "${CMAKE_SOURCE_DIR}/platforms/win/lib/"
        COMMENT "Copying bridge outputs to platforms/win/..."
    )
endif()

