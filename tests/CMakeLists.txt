project(Tests LANGUAGES C CXX)

get_filename_component(PARENT_DIR "../" ABSOLUTE)

#add_subdirectory(${PARENT_DIR}/engine Engine)

file(GLOB_RECURSE TESTS_CPP ${PROJECT_SOURCE_DIR}/src/*.cpp)

foreach(_file in ${TESTS_CPP})
	message(${_file})
endforeach()

enable_testing()

add_executable(
	test_engine
	${TESTS_CPP}
)
find_package(GTest CONFIG REQUIRED)
target_link_libraries(
  test_engine
  GTest::gtest GTest::gtest_main
  Engine
)

target_compile_features(test_engine PRIVATE cxx_std_20)
target_compile_features(test_engine PUBLIC cxx_std_20)
set_target_properties(test_engine PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(test_engine PROPERTIES CXX_EXTENSIONS OFF)

install(TARGETS test_engine DESTINATION ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/)

include(GoogleTest)
#gtest_add_tests(${PROJECT_NAME} ${TESTS_CPP})
gtest_discover_tests(test_engine)