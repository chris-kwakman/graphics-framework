project(Demo LANGUAGES C CXX)

get_filename_component(PARENT_DIR "../" ABSOLUTE)
get_filename_component(SRC_DIR "src" ABSOLUTE)

#add_subdirectory(${PARENT_DIR}/engine Engine)

link_directories(../lib/${CMAKE_BUILD_TYPE})

# Include all necessary .cpp files from includes

# Get demo .cpp files for compilation
file(GLOB_RECURSE DEMO_CPP_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)

list(APPEND LIST_CPP_FILES ${DEMO_CPP_FILES})

add_executable(${PROJECT_NAME} ${LIST_CPP_FILES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_link_libraries(${PROJECT_NAME} 
	PUBLIC Engine
)
target_include_directories(Demo PUBLIC "src/")

install(TARGETS ${PROJECT_NAME} DESTINATION bin/${CMAKE_BUILD_TYPE})

# Set proper C++ version

target_compile_features(Demo PUBLIC cxx_std_20)
set_target_properties(Demo PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(Demo PROPERTIES CXX_EXTENSIONS OFF)

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")