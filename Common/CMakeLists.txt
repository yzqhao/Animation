# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_LIB_SRCS 变量
include(../CPlatform.txt)

set(PROJECT_NAME Common)

################################################################################
# Source groups
################################################################################

set(__framework1
    "framework/icosphere.h"
    "framework/Application.h"
    "framework/Application.cpp"
    "framework/Camera.h"
    "framework/Camera.cpp"
    "framework/Immediate.h"
    "framework/Immediate.cpp"
    "framework/Image.h"
    "framework/Image.cpp"
    "framework/imgui.h"
    "framework/imgui_impl.h"
    "framework/imgui_impl.cpp"
    "framework/Options.h"
    "framework/Options.cpp"
    "framework/Mesh.h"
    "framework/Mesh.cpp"
    "framework/renderer.h"
    "framework/renderer_impl.h"
    "framework/renderer_impl.cpp"
    "framework/Shader.h"
    "framework/Shader.cpp"
    "framework/Shooter.h"
    "framework/Shooter.cpp"
    "framework/Profile.h"
    "framework/Profile.cpp"
)
source_group("framework" FILES ${__framework1})

set(__framework
    "framework/Shader.h"
    "framework/Shader.cpp"
    "framework/Renderer.h"
    "framework/Renderer.cpp"
    "framework/IWindow.h"
    "framework/Camera.h"
    "framework/Camera.cpp"
    "framework/Application.h"
    "framework/Application.cpp"
    "framework/PlaybackController.h"
    "framework/PlaybackController.cpp"
    "framework/Immediate.h"
    "framework/Immediate.cpp"
)
source_group("framework" FILES ${__framework})

set(__imgui
    "imgui/imconfig.h"
    "imgui/imgui.h"
    "imgui/imgui.cpp"
    "imgui/imgui_demo.cpp"
    "imgui/imgui_draw.cpp"
    "imgui/imgui_internal.h"
    "imgui/imgui_tables.cpp"
    "imgui/imgui_widgets.cpp"
    "imgui/imstb_rectpack.h"
    "imgui/imstb_textedit.h"
    "imgui/imstb_truetype.h"
)
source_group("imgui" FILES ${__imgui})

set(__imgui__backends
    "imgui/backends/imgui_impl_glfw.h"
    "imgui/backends/imgui_impl_glfw.cpp"
    "imgui/backends/imgui_impl_opengl2.h"
    "imgui/backends/imgui_impl_opengl2.cpp"
)
source_group("imgui\\backends" FILES ${__imgui__backends})

set(Source
    "BundleReader.cpp"
    "BundleReader.h"
    "Mesh.cpp"
    "Mesh.h"
    "SkinningJob.h"
    "SkinningJob.cpp"
    "Skeleton.cpp"
    "Skeleton.h"
    "skeleton_utils.cpp"
    "skeleton_utils.h"
    "Animation.cpp"
    "Animation.h"
    "RawAnimation.cpp"
    "RawAnimation.h"
    "RawAnimationJob.cpp"
    "RawAnimationJob.h"
    "AnimationJob.cpp"
    "AnimationJob.h"
    "LocalToModelJob.cpp"
    "LocalToModelJob.h"
    "BlendingJob.cpp"
    "BlendingJob.h"
    "LoadFile.cpp"
    "LoadFile.h"
    "span.h"
    "Utils.h"
    "Utils.cpp"
)
source_group("Source" FILES ${Source})

set(ALL_FILES
    ${__framework}
    ${__imgui}
    ${__imgui__backends}
    ${Source}
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${ROOT_PATH}/bin/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_EDITOR_DEBUG ${ROOT_PATH}/bin/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${ROOT_PATH}/bin/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_EDITOR_RELEASE ${ROOT_PATH}/bin/)

################################################################################
# Target
################################################################################
add_library(${PROJECT_NAME} STATIC ${ALL_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Engine")

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
        "${ROOT_PATH}/Third/glfw-3.3.5/include"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${ROOT_PATH}/Third"
        "${ROOT_PATH}/Third/glfw-3.3.5/include"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${ROOT_PATH}/Third"
        "${ROOT_PATH}/Third/glfw-3.3.5/include"
    )
endif()

################################################################################
# Dependencies
################################################################################
add_dependencies(${PROJECT_NAME}
	Math
	)
target_link_libraries(${PROJECT_NAME} PUBLIC
    Math
)	

################################################################################
# Compile and link options
################################################################################
include(../VSCompileLink.txt)

