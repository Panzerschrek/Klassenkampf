file(GLOB_RECURSE SOURCES "*.cpp" "*.hpp" "../Common/*.cpp" "../Common/*.hpp")
add_executable(ModelExporter ${SOURCES} ${SHADERS})
target_link_libraries(ModelExporter tinyxml2)
