# Search dependencies.
if(WIN32)
	set(SDL2_INCLUDE_DIRS ${SDL2_DIR}/include)
	set(SDL2_LIBRARIES ${SDL2_DIR}/lib/x64/SDL2.lib)
else()
	find_package(SDL2 REQUIRED)
endif()
find_package(Vulkan REQUIRED)
find_program(GLSLANGVALIDATOR glslangValidator)
if(NOT GLSLANGVALIDATOR)
	message(FATAL_ERROR "glslangValidator not found")
endif()

# Compile shaders.
file(GLOB SHADERS "shaders/*")
foreach(SHADER_FILE ${SHADERS})
	file(RELATIVE_PATH OUT_FILE ${CMAKE_CURRENT_SOURCE_DIR} ${SHADER_FILE})
	set(OUT_FILE_BASE ${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE})
	string(REPLACE ".glsl" ".h" OUT_FILE_H ${OUT_FILE_BASE})
	get_filename_component(VARIABLE_NAME ${SHADER_FILE} NAME)
	string(REPLACE ".glsl" "" VARIABLE_NAME ${VARIABLE_NAME})
	string(REPLACE "." "_" VARIABLE_NAME ${VARIABLE_NAME})
	add_custom_command(
		OUTPUT ${OUT_FILE_H}
		DEPENDS ${SHADER_FILE}
		COMMAND ${GLSLANGVALIDATOR} -V ${SHADER_FILE} --vn ${VARIABLE_NAME} -o ${OUT_FILE_H}
		)

	list(APPEND SHADERS_COMPILED ${OUT_FILE_H})
	set(SHADER_VARIABLES_LIST "${SHADER_VARIABLES_LIST}\t${VARIABLE_NAME},\n")
	set(SHADER_INCLUDES_LIST "${SHADER_INCLUDES_LIST}#include \"${OUT_FILE_H}\"\n")
	set(SHADER_LOOKUP_LIST "${SHADER_LOOKUP_LIST}PROCESS_SHADER(${VARIABLE_NAME})\n\t\t")
endforeach()

# Write shader list files.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ShaderList.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/ShaderList.hpp)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ShaderList.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/ShaderList.cpp)
set(GENERATED_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/ShaderList.hpp ${CMAKE_CURRENT_BINARY_DIR}/ShaderList.cpp)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/ShaderList.cpp PROPERTIES OBJECT_DEPENDS ${SHADERS_COMPILED})

# Add target Klassenkampf.

file(GLOB_RECURSE SOURCES "*.cpp" "*.hpp")
add_executable(Klassenkampf ${SOURCES} ${GENERATED_SOURCES} ${SHADERS} ${SHADERS_COMPILED})
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
