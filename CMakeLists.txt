cmake_minimum_required(VERSION 3.15)

project(root)

enable_language(CXX)

if ((MSVC) AND (MSVC_VERSION GREATER_EQUAL 1914))
    add_compile_options("/Zc:__cplusplus")
endif()


# Include other project subdirectories
add_subdirectory(engine framework_engine)

add_subdirectory(demo framework_demo)

#enable_testing()
#add_subdirectory(tests framework_tests)