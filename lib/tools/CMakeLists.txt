set(target_name dalbaragi_tools)
set(public_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(private_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(private_source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(public_header_files
    ${public_header_dir}/daltools/dmd/exporter.h
    ${public_header_dir}/daltools/dsd/importer.hpp
    ${public_header_dir}/daltools/dsd/struct.hpp
    ${public_header_dir}/daltools/json/parser.h
    ${public_header_dir}/daltools/scene/mesh_opt.hpp
    ${public_header_dir}/daltools/scene/modifier.h
)

set(private_source_files
    ${private_source_dir}/dmd/exporter.cpp
    ${private_source_dir}/dsd/importer.cpp
    ${private_source_dir}/dsd/struct.cpp
    ${private_source_dir}/json/parser.cpp
    ${private_source_dir}/scene/mesh_opt.cpp
    ${private_source_dir}/scene/modifier.cpp
    ${private_source_dir}/scene/modifier_scene.cpp
    ${private_source_dir}/scene/struct.cpp
)


add_library(${target_name} STATIC)
add_library(dalbaragi::${target_name} ALIAS ${target_name})
set_target_properties(${target_name} PROPERTIES FOLDER "dalbaragi")

target_sources(${target_name}
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS
            ${public_header_dir}
        FILES
            ${public_header_files}
    PRIVATE
        ${private_header_files}
        ${private_source_files}
)

target_include_directories(${target_name}
    PUBLIC
        ${public_header_dir}
    PRIVATE
        ${private_header_dir}
)

target_link_libraries(${target_name}
    PUBLIC
        dalbaragi::dalbaragi_parser
        meshoptimizer::meshoptimizer
        nlohmann_json::nlohmann_json
        unofficial::brotli::brotlienc
        yaml-cpp::yaml-cpp
)
