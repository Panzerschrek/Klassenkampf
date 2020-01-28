file(GLOB_RECURSE SOURCES "*.cpp" "*.hpp")
add_library(Common ${SOURCES} ${SHADERS})
