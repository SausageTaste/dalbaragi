add_executable(daltools
    ops/work_batch.cpp
    ops/work_bundle.cpp
    ops/work_compile.cpp
    ops/work_key.cpp
    ops/work_keygen.cpp
    main.cpp
)
target_include_directories(daltools PUBLIC .)
target_compile_features(daltools PUBLIC cxx_std_17)
set_target_properties(daltools PROPERTIES
    FOLDER "dalbaragi/app"
)


######## Import libraries ########

target_link_libraries(daltools
    PRIVATE
        dalbaragi::dalbaragi_tools
        argparse::argparse
)
