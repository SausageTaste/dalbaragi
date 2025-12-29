set(target_name dalbaragi_aux)
set(public_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(private_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(private_source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(public_header_files
    ${public_header_dir}/dal/auxiliary/bin_data.hpp
    ${public_header_dir}/dal/auxiliary/byte_tool.hpp
    ${public_header_dir}/dal/auxiliary/err_msg_holder.hpp
    ${public_header_dir}/dal/auxiliary/glm_tool.hpp
    ${public_header_dir}/dal/auxiliary/path.hpp
    ${public_header_dir}/dal/auxiliary/timer.hpp
    ${public_header_dir}/dal/auxiliary/util.hpp
)

set(private_source_files
    ${private_source_dir}/byte_tool.cpp
    ${private_source_dir}/err_msg_holder.cpp
    ${private_source_dir}/path.cpp
    ${private_source_dir}/timer.cpp
    ${private_source_dir}/util.cpp
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
    PRIVATE
        ${private_header_dir}
)

target_link_libraries(${target_name} PUBLIC
    glm::glm
    spdlog::spdlog
    sungtools::sungtools_basic
)
