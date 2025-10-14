set(source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(source_files
    ${source_dir}/bundle/bundle.cpp
    ${source_dir}/bundle/repo.cpp
    ${source_dir}/common/byte_tool.cpp
    ${source_dir}/common/compress.cpp
    ${source_dir}/common/decompress.cpp
    ${source_dir}/common/err_msg_holder.cpp
    ${source_dir}/common/task_sys.cpp
    ${source_dir}/common/util.cpp
    ${source_dir}/dmd/parser.cpp
    ${source_dir}/filesys/filesys.cpp
    ${source_dir}/filesys/res_mgr.cpp
    ${source_dir}/img/backend/ktx.cpp
    ${source_dir}/img/backend/stb.cpp
    ${source_dir}/img/img.cpp
    ${source_dir}/scene/struct.cpp
)


add_library(dalbaragi_parser STATIC ${source_files})
add_library(dalbaragi::dalbaragi_parser ALIAS dalbaragi_parser)
target_compile_features(dalbaragi_parser PUBLIC cxx_std_17)

target_include_directories(dalbaragi_parser PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Stb_INCLUDE_DIR}
)

target_link_libraries(dalbaragi_parser PUBLIC
    aklomp::base64
    enkiTS::enkiTS
    glm::glm
    KTX::ktx
    spdlog::spdlog
    sungtools::sungtools_basic
    unofficial::brotli::brotlidec
    ZLIB::ZLIB
)
