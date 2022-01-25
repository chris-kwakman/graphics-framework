cmake_minimum_required(VERSION 3.16)
project(Demo LANGUAGES C CXX)

# Ensure we use proper C++ standard
set(CMAKE_CXX_STANDARDS 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

get_filename_component(PARENT_DIR "../" ABSOLUTE)
get_filename_component(SRC_DIR "src" ABSOLUTE)

add_subdirectory(${PARENT_DIR}/engine Engine)

link_directories(../lib/${CMAKE_BUILD_TYPE})
include_directories(
	"${PARENT_DIR}/include/"
	"${PARENT_DIR}/engine/src"
	"src/"
)

# Include all necessary .cpp files from includes

# Get demo .cpp files for compilation
file(GLOB_RECURSE DEMO_CPP_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
# Get include .cpp files for compilation
file(GLOB IMGUI_CPP ${PARENT_DIR}/include/ImGui/*.cpp)
set(IMGUIZMO_CPP ${PARENT_DIR}/include/ImGuizmo/ImGuizmo.cpp)
#file(GLOB_RECURSE INCLUDE_CPP_FILES ${PARENT_DIR}/include/*.cpp)

list(APPEND LIST_CPP_FILES ${DEMO_CPP_FILES})
list(APPEND LIST_CPP_FILES ${IMGUI_CPP})
list(APPEND LIST_CPP_FILES ${IMGUIZMO_CPP})

add_executable(${PROJECT_NAME} ${LIST_CPP_FILES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_link_libraries(${PROJECT_NAME} 
	PUBLIC Engine
	PUBLIC opengl32 glew32 SDL2 SDL2main
)

install(TARGETS ${PROJECT_NAME} DESTINATION ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/)

