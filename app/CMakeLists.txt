cmake_minimum_required(VERSION 3.10)


add_executable(daltools
    main.cpp
    work_compile.cpp
    work_key.cpp
    work_keygen.cpp
)
target_include_directories(daltools PUBLIC .)
target_compile_features(daltools PUBLIC cxx_std_17)


######## Import libraries ########

target_link_libraries(daltools
    PRIVATE
        dalbaragi::daltools
        argparse::argparse
)
