# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_LIB_SRCS 变量
include(../../CPlatform.txt)

set(CMAKE_CXX_STANDARD 14)  

set(PROJECT_NAME stb_image)

################################################################################
# Source groups
################################################################################

file(
    GLOB ALL_FILES
    "stb_image.cpp"
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${ROOT_PATH}/bin/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${ROOT_PATH}/bin/)

################################################################################
# Target
################################################################################
add_library(${PROJECT_NAME} STATIC ${ALL_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Third")
	
################################################################################
# Target name
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Any CPU")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        TARGET_NAME_DEBUG          "${PROJECT_NAME}_d"
        TARGET_NAME_EDITOR_DEBUG   "${PROJECT_NAME}_d"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        TARGET_NAME_DEBUG          "${PROJECT_NAME}_d"
        TARGET_NAME_EDITOR_DEBUG   "${PROJECT_NAME}_d"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        TARGET_NAME_DEBUG          "${PROJECT_NAME}_d"
        TARGET_NAME_EDITOR_DEBUG   "${PROJECT_NAME}_d"
    )
endif()

################################################################################
# Include directories
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Any CPU")
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${ROOT_PATH}/Third"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${ROOT_PATH}/Third"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${ROOT_PATH}/Third"
    )
endif()

################################################################################
# Compile and link options
################################################################################
include(../../VSCompileLink.txt)
