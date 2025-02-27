cmake_minimum_required(VERSION 3.11.0)

if (NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

project(dalbaragi
    VERSION 0.1.2
    LANGUAGES CXX
)


set(DAL_USE_SUNGTOOLS_SUBMODULE ON CACHE BOOL "Use sungtools as a submodule")
set(DAL_BUILD_APP OFF CACHE BOOL "Build projects in app folder")
set(DAL_ENABLE_TEST OFF CACHE BOOL "Enable testing")

set(source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)


# Define external libraries
# ----------------------------------------------------------------------------------

if (DAL_USE_SUNGTOOLS_SUBMODULE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/extern/sungtools)
else()
    find_package(sungtools CONFIG REQUIRED)
endif()

find_package(argparse CONFIG REQUIRED)
find_package(base64 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Ktx CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Stb MODULE REQUIRED)
find_package(unofficial-brotli CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

# Define project library
# ----------------------------------------------------------------------------------

add_library(dalbaragi_tools STATIC
    ${source_dir}/bundle/bundle.cpp
    ${source_dir}/bundle/repo.cpp
    ${source_dir}/common/byte_tool.cpp
    ${source_dir}/common/compression.cpp
    ${source_dir}/common/util.cpp
    ${source_dir}/dmd/exporter.cpp
    ${source_dir}/dmd/parser.cpp
    ${source_dir}/filesys/filesys.cpp
    ${source_dir}/filesys/res_mgr.cpp
    ${source_dir}/img/backend/ktx.cpp
    ${source_dir}/img/backend/stb.cpp
    ${source_dir}/img/img.cpp
    ${source_dir}/json/parser.cpp
    ${source_dir}/scene/modifier_scene.cpp
    ${source_dir}/scene/modifier.cpp
    ${source_dir}/scene/struct.cpp
)
target_include_directories(dalbaragi_tools PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${Stb_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(dalbaragi_tools PUBLIC cxx_std_17)
add_library(dalbaragi::dalbaragi_tools ALIAS dalbaragi_tools)


# Import external libraries
# ----------------------------------------------------------------------------------

target_link_libraries(dalbaragi_tools PUBLIC
    aklomp::base64
    glm::glm
    KTX::ktx
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    sungtools::sungtools_basic
    unofficial::brotli::brotlidec
    unofficial::brotli::brotlienc
    yaml-cpp::yaml-cpp
    ZLIB::ZLIB
)


# Install
# ----------------------------------------------------------------------------------

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION share/${PROJECT_NAME}
)

install(TARGETS dalbaragi_tools
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION $<IF:$<CONFIG:Debug>,debug/lib,lib>
    ARCHIVE DESTINATION $<IF:$<CONFIG:Debug>,debug/lib,lib>
    RUNTIME DESTINATION $<IF:$<CONFIG:Debug>,debug/bin,bin>
    INCLUDES DESTINATION include
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION share/${PROJECT_NAME}
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION share/${PROJECT_NAME}
)


# Add subdirectories
# ----------------------------------------------------------------------------------

if (DAL_BUILD_APP)
    add_subdirectory(./app)
endif()

if (DAL_ENABLE_TEST)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(./test)
endif()
