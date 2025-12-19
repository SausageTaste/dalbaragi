set(target_name dalbaragi_tools)
set(public_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(private_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(private_source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB_RECURSE public_header_files CONFIGURE_DEPENDS
    "${public_header_dir}/*.h"
    "${public_header_dir}/*.hpp"
)

file(GLOB_RECURSE private_header_files CONFIGURE_DEPENDS
    "${private_header_dir}/*.h"
    "${private_header_dir}/*.hpp"
)

file(GLOB_RECURSE private_source_files CONFIGURE_DEPENDS
    "${private_source_dir}/*.cpp"
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
