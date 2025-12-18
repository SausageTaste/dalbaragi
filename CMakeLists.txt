cmake_minimum_required(VERSION 3.11.0)

if (NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

project(dalbaragi
    VERSION 0.1.2
    LANGUAGES CXX
)


set(DAL_BUILD_TOOLS_LIB OFF CACHE BOOL "Build dalbaragi tools static library")
set(DAL_BUILD_APP OFF CACHE BOOL "Build projects in app folder")
set(DAL_BUILD_TEST OFF CACHE BOOL "Enable testing")

if (DAL_BUILD_APP OR DAL_BUILD_TEST)
    set(DAL_BUILD_TOOLS_LIB ON)
endif()


# ------------------------------------------------------------------------------

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/extern/sungtools)

find_package(base64 CONFIG REQUIRED)
find_package(enkiTS CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Ktx CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Stb MODULE REQUIRED)
find_package(unofficial-brotli CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

add_subdirectory(lib/auxiliary)
add_subdirectory(lib/parser)


# ------------------------------------------------------------------------------

if (DAL_BUILD_TOOLS_LIB)
    find_package(meshoptimizer CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(yaml-cpp CONFIG REQUIRED)
    add_subdirectory(lib/tools)
endif()

if (DAL_BUILD_APP)
    find_package(argparse CONFIG REQUIRED)
    add_subdirectory(./app)
endif()

if (DAL_BUILD_TEST)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(./test)
endif()
