cmake_minimum_required(VERSION 3.10)

project(Klassenkampf)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

find_package(SDL2 REQUIRED)

file(GLOB SOURCES "*.cpp" "*.hpp")

add_executable(Klassenkampf ${SOURCES})
target_include_directories(Klassenkampf PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(Klassenkampf  ${SDL2_LIBRARIES})
