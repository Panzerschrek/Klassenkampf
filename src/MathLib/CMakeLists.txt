file(GLOB_RECURSE SOURCES "*.cpp" "*.hpp")
add_library(MathLib ${SOURCES} ${SHADERS})
