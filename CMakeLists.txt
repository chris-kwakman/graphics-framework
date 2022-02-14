cmake_minimum_required(VERSION 3.15)

enable_testing()

project(root)

enable_language(CXX)

add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)

if ((MSVC) AND (MSVC_VERSION GREATER_EQUAL 1914))
    add_compile_options("/Zc:__cplusplus")
endif()

# Include other project subdirectories
add_subdirectory(engine framework_engine)
add_subdirectory(demo framework_demo)
add_subdirectory(tests framework_tests)