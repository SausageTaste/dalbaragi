set(target_name dalbaragi_parser)
set(public_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(private_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(private_source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(public_header_files
    ${public_header_dir}/daltools/bundle/bundle.hpp
    ${public_header_dir}/daltools/bundle/repo.hpp
    ${public_header_dir}/daltools/common/bin_data.h
    ${public_header_dir}/daltools/common/byte_tool.h
    ${public_header_dir}/daltools/common/compress.hpp
    ${public_header_dir}/daltools/common/decompress.hpp
    ${public_header_dir}/daltools/common/err_msg_holder.hpp
    ${public_header_dir}/daltools/common/glm_tool.hpp
    ${public_header_dir}/daltools/common/konst.h
    ${public_header_dir}/daltools/common/task_sys.hpp
    ${public_header_dir}/daltools/common/util.h
    ${public_header_dir}/daltools/dmd/parser.h
    ${public_header_dir}/daltools/filesys/filesys.hpp
    ${public_header_dir}/daltools/filesys/path.hpp
    ${public_header_dir}/daltools/filesys/res_mgr.hpp
    ${public_header_dir}/daltools/img/backend/ktx.hpp
    ${public_header_dir}/daltools/img/backend/stb.hpp
    ${public_header_dir}/daltools/img/img.hpp
    ${public_header_dir}/daltools/img/img2d.hpp
    ${public_header_dir}/daltools/scene/struct.h
)

set(private_source_files
    ${private_source_dir}/bundle/bundle.cpp
    ${private_source_dir}/bundle/repo.cpp
    ${private_source_dir}/common/byte_tool.cpp
    ${private_source_dir}/common/compress.cpp
    ${private_source_dir}/common/decompress.cpp
    ${private_source_dir}/common/err_msg_holder.cpp
    ${private_source_dir}/common/task_sys.cpp
    ${private_source_dir}/common/util.cpp
    ${private_source_dir}/dmd/parser.cpp
    ${private_source_dir}/filesys/filesys.cpp
    ${private_source_dir}/filesys/path.cpp
    ${private_source_dir}/filesys/res_mgr.cpp
    ${private_source_dir}/img/backend/ktx.cpp
    ${private_source_dir}/img/backend/stb.cpp
    ${private_source_dir}/img/img.cpp
    ${private_source_dir}/scene/struct.cpp
)


add_library(${target_name} STATIC)
add_library(dalbaragi::${target_name} ALIAS ${target_name})
set_target_properties(${target_name} PROPERTIES FOLDER "dalbaragi")
target_compile_features(${target_name} PUBLIC cxx_std_20)

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
        ${Stb_INCLUDE_DIR}
    PRIVATE
        ${private_header_dir}
)

target_link_libraries(${target_name}
    PUBLIC
        aklomp::base64
        enkiTS::enkiTS
        glm::glm
        KTX::ktx
        spdlog::spdlog
        sungtools::sungtools_basic
        unofficial::brotli::brotlidec
        ZLIB::ZLIB
)
