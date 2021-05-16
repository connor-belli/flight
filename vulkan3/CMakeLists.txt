cmake_minimum_required (VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan)
find_package(imgui CONFIG REQUIRED)
find_package(SDL2 REQUIRED)
find_package(Bullet CONFIG REQUIRED)
find_package(RapidJson REQUIRED)
find_package(sdl2-mixer CONFIG REQUIRED)
find_package(OpenXR CONFIG REQUIRED)


include_directories(engine ${Vulkan_INCLUDE_DIRS})
include_directories(engine ${BULLET_INCLUDE_DIRS})
set(KERNELS
	"color.frag"
	"color.vert"
	"shadow.vert"
)

set(MODELS

)

set(OUTPUT_MODELS

)

set(SCENES

)

set(OUTPUT_SCENES

)

set(COMPILED_KERNELS
	"color.frag.spv"
	"color.vert.spv"
	"shadow.vert.spv"
)

set(HEADERS
	"vulkan3.h"
	"pch.h"
	"vkctx.h"
	"defaultpipeline.h"
	"defaultvertex.h"
	"vertexbuffer.h"
	"imgui_imple.h"
	"defaultrenderpass.h"
	"vkimage.h"
	"framedata.h"  "model.h"    "physics.h" "shadowpipeline.h" "shadowrenderpass.h")

set(IMPLEMENTATIONS
	"vulkan3.cpp"
	"pch.cpp"
	"vkctx.cpp"
	"defaultpipeline.cpp"
	"imgui_imple.cpp"
	"defaultrenderpass.cpp"
	"vkimage.cpp"
	"framedata.cpp"  "shadowpipeline.cpp" "shadowrenderpass.cpp" "vrctx.cpp")

add_executable (engine 
	${HEADERS}
	${IMPLEMENTATIONS}
	${KERNELS}
	${MODELS}
	${OUTPUT_MODELS}
	${SCENES}
	${OUTPUT_SCENES}
	${COMPILED_KERNELS}
  "vrctx.h")

foreach(KERNEL ${KERNELS})
	add_custom_command(OUTPUT ${KERNEL}.spv
	COMMAND C:/VulkanSDK/1.2.154.1/Bin/glslc ${CMAKE_CURRENT_SOURCE_DIR}/${KERNEL} -g -o "${CMAKE_CURRENT_BINARY_DIR}/${KERNEL}.spv"
	DEPENDS ${KERNEL}
	COMMENT "Rebuilding ${KERNEL}.spv" )
	message(STATUS "Generating build commands for ${KERNEL}.spv")
endforeach()

foreach(MODEL ${MODELS})
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${MODEL}
	COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${MODEL}" "${CMAKE_CURRENT_BINARY_DIR}/${MODEL}"
	DEPENDS ${MODEL})
endforeach()

foreach(SCENE ${SCENES})
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${SCENE}
	COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${SCENE}" "${CMAKE_CURRENT_BINARY_DIR}/${SCENE}"
	DEPENDS ${SCENE})
endforeach()

target_precompile_headers(engine PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/vkctx.h>"
)

target_link_libraries(engine PRIVATE ${Vulkan_LIBRARIES})
target_link_libraries(engine PRIVATE LinearMath Bullet3Common BulletDynamics BulletCollision BulletSoftBody)
target_link_libraries(engine PRIVATE imgui::imgui)
target_include_directories(engine PRIVATE ${RAPIDJSON_INCLUDE_DIRS})
target_link_libraries(engine PRIVATE SDL2::SDL2 SDL2::SDL2main)
target_link_libraries(engine PRIVATE SDL2::SDL2_mixer)
target_link_libraries(engine PRIVATE OpenXR::headers OpenXR::openxr_loader)
# TODO: Add tests and install targets if needed.