cmake_minimum_required(VERSION 3.10)

project(Klassenkampf)

# Setup language options.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

# Setup compiler options.
if(MSVC)
	add_compile_options(/utf-8 /W3 /WX)
	add_definitions(-DNOMINMAX)
else()
	add_compile_options(-Werror -Wall -Wextra -Wconversion)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DDEBUG)
endif()

# Setup TinyXML2 library. Build static library, disable tests.
set(BUILD_SHARED_LIBS CACHE BOOl "" FORCE)
set(BUILD_TESTS CACHE BOOl "" FORCE)
add_subdirectory(tinyxml2)

add_subdirectory(Common)
add_subdirectory(MathLib)
add_subdirectory(Game)
add_subdirectory(ModelExporter)
