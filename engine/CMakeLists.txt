project(Engine LANGUAGES C CXX)

get_filename_component(PARENT_DIR "../" ABSOLUTE)

set(ENGINE_SRC_DIR ${PROJECT_SOURCE_DIR}/src)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

include_directories(${PARENT_DIR}/include)

# Collect engine library source and header files
file(GLOB_RECURSE ENGINE_LIB_H_FILES ${PROJECT_SOURCE_DIR}/src *.h *.hpp)
file(GLOB_RECURSE ENGINE_LIB_CPP_FILES ${PROJECT_SOURCE_DIR}/src *.cpp)
set(DELAUNATOR_CPP ${PARENT_DIR}/include/delaunator/delaunator.cpp)

add_library(${PROJECT_NAME} ${ENGINE_LIB_CPP_FILES} ${DELAUNATOR_CPP})

target_include_directories(${PROJECT_NAME} PUBLIC ${ENGINE_SRC_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC ${PARENT_DIR}/include)


# Find includes / libraries

find_package(GLEW REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
find_path(STB_INCLUDE_DIRS "stb.h")

target_include_directories(Engine PUBLIC ${TINYGLTF_INCLUDE_DIRS})
target_include_directories(Engine PUBLIC ${STB_INCLUDE_DIRS})

target_link_libraries(Engine PUBLIC 
	GLEW::GLEW
	SDL2::SDL2 SDL2::SDL2main
	glm::glm
	imgui::imgui
	imguizmo::imguizmo
	nlohmann_json nlohmann_json::nlohmann_json
)

# Set proper C++ version

target_compile_features(Engine PUBLIC cxx_std_17)
set_target_properties(Engine PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(Engine PROPERTIES CXX_EXTENSIONS OFF)

target_compile_definitions(Engine PUBLIC -DIMGUI_USER_CONFIG="${ENGINE_SRC_DIR}/Engine/Editor/my_imgui_config.h")

# Install Engine library

install(TARGETS ${PROJECT_NAME} DESTINATION ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/)