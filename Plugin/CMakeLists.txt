cmake_minimum_required(VERSION 3.21)

project(DAGGER VERSION 1.0.1 LANGUAGES CXX)

# Version comes from project() above; bump the revision on every build handed to a tester. Both are shown in the log,\n# the DLL version, the FOMOD, the ini and the zip name.
set(DAGGER_REVISION 7)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CommonLibSSE-NG (alandtse fork, v15.5.0), a git submodule. Override with -DCOMMONLIB_DIR=... to use another checkout.
set(COMMONLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG" CACHE PATH "CommonLibSSE-NG source dir")

set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${COMMONLIB_DIR} ${CMAKE_CURRENT_BINARY_DIR}/commonlib)
include(${COMMONLIB_DIR}/cmake/CommonLibSSE.cmake)

# Plugin info is declared in src/main.cpp (the helper cannot set the post-1.6.629 struct flag).
add_library(DAGGER SHARED src/main.cpp)
target_link_libraries(DAGGER PUBLIC CommonLibSSE::CommonLibSSE)
target_compile_definitions(DAGGER PRIVATE __CMAKE_COMMONLIBSSE_PLUGIN=1 NOMINMAX WIN32_LEAN_AND_MEAN DAGGER_REVISION=${DAGGER_REVISION} DAGGER_VER_MAJOR=${PROJECT_VERSION_MAJOR} DAGGER_VER_MINOR=${PROJECT_VERSION_MINOR} DAGGER_VER_PATCH=${PROJECT_VERSION_PATCH})
target_include_directories(DAGGER PRIVATE src)
if(MSVC)
	target_compile_options(DAGGER PRIVATE /utf-8 /permissive- /Zc:preprocessor /EHsc "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/ForceInclude.h")
endif()
