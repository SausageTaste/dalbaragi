set(source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(source_files
    ${source_dir}/dmd/exporter.cpp
    ${source_dir}/dsd/importer.cpp
    ${source_dir}/dsd/struct.cpp
    ${source_dir}/json/parser.cpp
    ${source_dir}/scene/mesh_opt.cpp
    ${source_dir}/scene/modifier_scene.cpp
    ${source_dir}/scene/modifier.cpp
)


add_library(dalbaragi_tools STATIC ${source_files})
add_library(dalbaragi::dalbaragi_tools ALIAS dalbaragi_tools)
target_compile_features(dalbaragi_tools PUBLIC cxx_std_17)

target_include_directories(dalbaragi_tools PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(dalbaragi_tools PUBLIC
    dalbaragi::dalbaragi_parser
    meshoptimizer::meshoptimizer
    nlohmann_json::nlohmann_json
    unofficial::brotli::brotlienc
    yaml-cpp::yaml-cpp
)
set_target_properties(dalbaragi_tools PROPERTIES
    FOLDER "dalbaragi"
)
