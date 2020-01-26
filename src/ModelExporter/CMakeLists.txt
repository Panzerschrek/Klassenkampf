file(GLOB_RECURSE SOURCES "*.cpp" "*.hpp")
add_executable(ModelExporter ${SOURCES} ${SHADERS})
target_link_libraries(
	ModelExporter
	Common
	MathLib
	tinyxml2)
