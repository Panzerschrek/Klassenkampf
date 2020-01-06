cmake_minimum_required(VERSION 3.10)

project(Klassenkampf)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

if(MSVC)
	add_compile_options(/utf-8 /W3 /Wx)
else()
	add_compile_options(-Werror -Wall -Wextra -Wconversion)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DDEBUG)
endif()

find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)

file(GLOB SOURCES "*.cpp" "*.hpp")

add_executable(Klassenkampf ${SOURCES})

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
