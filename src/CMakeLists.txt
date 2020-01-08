cmake_minimum_required(VERSION 3.10)

project(Klassenkampf)

# Setup language options.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

# Setup compiler options.
if(MSVC)
	add_compile_options(/utf-8 /W3 /Wx)
else()
	add_compile_options(-Werror -Wall -Wextra -Wconversion)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DDEBUG)
endif()

# Search dependencies.
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
find_program(GLSLANGVALIDATOR glslangValidator)
if(NOT GLSLANGVALIDATOR)
	message(FATAL_ERROR "glslangValidator not found")
endif()

# Compile shaders.
function(CompileShader IN_FILE OUT_FILE_DST)
	file(RELATIVE_PATH OUT_FILE ${CMAKE_CURRENT_SOURCE_DIR} ${IN_FILE})
	set(OUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE})
	string(REPLACE ".glsl" ".sprv" OUT_FILE ${OUT_FILE})
	add_custom_command(
		OUTPUT ${OUT_FILE}
		DEPENDS ${IN_FILE}
		COMMAND ${GLSLANGVALIDATOR} -V ${IN_FILE} -o ${OUT_FILE}
		)
	set(${OUT_FILE_DST} ${OUT_FILE} PARENT_SCOPE)
endfunction()

file(GLOB SHADERS "shaders/*")
foreach(SHADER_FILE ${SHADERS})
	CompileShader(${SHADER_FILE} SHADER_FILE_OUT)
	list(APPEND SHADERS_COMPILED ${SHADER_FILE_OUT})
endforeach()

# Add target Klassenkampf.

file(GLOB SOURCES "*.cpp" "*.hpp")
add_executable(Klassenkampf ${SOURCES} ${SHADERS} ${SHADERS_COMPILED})

target_include_directories(
	Klassenkampf
		PRIVATE
			${SDL2_INCLUDE_DIRS}
			${Vulkan_INCLUDE_DIRS}
		)

target_link_libraries(
	Klassenkampf
		${SDL2_LIBRARIES}
		${Vulkan_LIBRARIES}
	)
