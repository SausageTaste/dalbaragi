set(target_name dalbaragi_tools)
set(public_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(private_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(private_source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(public_header_files
    ${public_header_dir}/dal/bundle/bundle.hpp
    ${public_header_dir}/dal/bundle/repo.hpp
    ${public_header_dir}/dal/common/compress.hpp
    ${public_header_dir}/dal/common/decompress.hpp
    ${public_header_dir}/dal/common/konst.hpp
    ${public_header_dir}/dal/common/task_sys.hpp
    ${public_header_dir}/dal/dmd/exporter.hpp
    ${public_header_dir}/dal/dmd/parser.hpp
    ${public_header_dir}/dal/dsd/importer.hpp
    ${public_header_dir}/dal/dsd/struct.hpp
    ${public_header_dir}/dal/filesys/filesys.hpp
    ${public_header_dir}/dal/filesys/res_mgr.hpp
    ${public_header_dir}/dal/img/backend/ktx.hpp
    ${public_header_dir}/dal/img/backend/stb.hpp
    ${public_header_dir}/dal/img/img.hpp
    ${public_header_dir}/dal/img/img2d.hpp
    ${public_header_dir}/dal/json/parser.hpp
    ${public_header_dir}/dal/scene/mesh_opt.hpp
    ${public_header_dir}/dal/scene/modifier.hpp
    ${public_header_dir}/dal/scene/struct.hpp
)

set(private_source_files
    ${private_source_dir}/bundle/bundle.cpp
    ${private_source_dir}/bundle/repo.cpp
    ${private_source_dir}/common/compress.cpp
    ${private_source_dir}/common/decompress.cpp
    ${private_source_dir}/common/task_sys.cpp
    ${private_source_dir}/dmd/exporter.cpp
    ${private_source_dir}/dmd/parser.cpp
    ${private_source_dir}/dsd/importer.cpp
    ${private_source_dir}/dsd/struct.cpp
    ${private_source_dir}/filesys/filesys.cpp
    ${private_source_dir}/filesys/res_mgr.cpp
    ${private_source_dir}/img/backend/ktx.cpp
    ${private_source_dir}/img/backend/stb.cpp
    ${private_source_dir}/img/img.cpp
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
        ${Stb_INCLUDE_DIR}
)

target_link_libraries(${target_name}
    PUBLIC
        dalbaragi::dalbaragi_aux
        enkiTS::enkiTS
        KTX::ktx
    PRIVATE
        aklomp::base64
        meshoptimizer::meshoptimizer
        nlohmann_json::nlohmann_json
        unofficial::brotli::brotlidec
        unofficial::brotli::brotlienc
        yaml-cpp::yaml-cpp
        ZLIB::ZLIB
)
