cmake_minimum_required(VERSION 3.16)
project(Engine LANGUAGES C CXX)

get_filename_component(PARENT_DIR "../" ABSOLUTE)

# Ensure we use proper C++ standard
set(CMAKE_CXX_STANDARDS 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(ENGINE_SRC ${PROJECT_SOURCE_DIR}/src)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

link_directories(${PARENT_DIR}/lib/${CMAKE_BUILD_TYPE})
include_directories(${PARENT_DIR}/include)

# Collect engine library source and header files
file(GLOB_RECURSE ENGINE_LIB_H_FILES ${PROJECT_SOURCE_DIR}/src *.h *.hpp)
file(GLOB_RECURSE ENGINE_LIB_CPP_FILES ${PROJECT_SOURCE_DIR}/src *.cpp)
set(DELAUNATOR_CPP ${PARENT_DIR}/include/delaunator/delaunator.cpp)

add_library(${PROJECT_NAME} ${ENGINE_LIB_CPP_FILES} ${DELAUNATOR_CPP})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_link_libraries(${PROJECT_NAME} PUBLIC opengl32 glew32 SDL2 SDL2main)
target_include_directories(${PROJECT_NAME} PUBLIC ${ENGINE_SRC})
target_include_directories(${PROJECT_NAME} PUBLIC ${PARENT_DIR}/include)

install(TARGETS ${PROJECT_NAME} DESTINATION ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/)