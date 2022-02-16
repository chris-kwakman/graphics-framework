project(Tests LANGUAGES C CXX)

get_filename_component(PARENT_DIR "../" ABSOLUTE)

#add_subdirectory(${PARENT_DIR}/engine Engine)

file(GLOB_RECURSE TESTS_CPP ${PROJECT_SOURCE_DIR}/src/*.cpp)

foreach(_file in ${TESTS_CPP})
	message(${_file})
endforeach()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(
	"${PARENT_DIR}/engine/src"
	"${PARENT_DIR}/include"
)
link_directories(${PARENT_DIR}/lib/${CMAKE_BUILD_TYPE})

enable_testing()

add_executable(
	${PROJECT_NAME} 
	${TESTS_CPP}
)
target_link_libraries(
  ${PROJECT_NAME}
  gtest_main
  Engine
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_compile_features(Tests PUBLIC cxx_std_20)
set_target_properties(Tests PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(Tests PROPERTIES CXX_EXTENSIONS OFF)

install(TARGETS ${PROJECT_NAME} DESTINATION ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/)

include(GoogleTest)
#gtest_add_tests(${PROJECT_NAME} ${TESTS_CPP})
gtest_discover_tests(${PROJECT_NAME})