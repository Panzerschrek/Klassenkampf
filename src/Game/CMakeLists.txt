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
	set(OUT_FILE_BASE ${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE})
	string(REPLACE ".glsl" ".sprv" OUT_FILE_SPRV ${OUT_FILE_BASE})
	string(REPLACE ".glsl" ".sprv.h" OUT_FILE_H ${OUT_FILE_BASE})
	get_filename_component(VARIABLE_NAME ${IN_FILE} NAME)
	string(REPLACE ".glsl" "" VARIABLE_NAME ${VARIABLE_NAME})
	string(REPLACE "." "_" VARIABLE_NAME ${VARIABLE_NAME})
	add_custom_command(
		OUTPUT ${OUT_FILE_SPRV}
		DEPENDS ${IN_FILE}
		COMMAND ${GLSLANGVALIDATOR} -V ${IN_FILE} -o ${OUT_FILE_SPRV}
		)
	add_custom_command(
		OUTPUT ${OUT_FILE_H}
		DEPENDS ${OUT_FILE_SPRV}
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/bin2c.cmake ${OUT_FILE_SPRV} ${OUT_FILE_H} c_${VARIABLE_NAME}_file_content
		)
	set(${OUT_FILE_DST} ${OUT_FILE_H} PARENT_SCOPE)
endfunction()

file(GLOB SHADERS "shaders/*")
foreach(SHADER_FILE ${SHADERS})
	CompileShader(${SHADER_FILE} SHADER_FILE_OUT)
	list(APPEND SHADERS_COMPILED ${SHADER_FILE_OUT})
endforeach()

# Add target Klassenkampf.

file(GLOB_RECURSE SOURCES "*.cpp" "*.hpp")
add_executable(Klassenkampf ${SOURCES} ${SHADERS} ${SHADERS_COMPILED})
target_include_directories(Klassenkampf PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(
	Klassenkampf
		PRIVATE
			${SDL2_INCLUDE_DIRS}
			${Vulkan_INCLUDE_DIRS}
		)

target_link_libraries(
	Klassenkampf
		Common
		MathLib
		${SDL2_LIBRARIES}
		${Vulkan_LIBRARIES}
	)
