#include <gtest/gtest.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/xchar.h>
#include <sung/basic/stringtool.hpp>

#include "dal/parser/filesys/filesys.hpp"
#include "dal/parser/filesys/res_mgr.hpp"
#include "dal/parser/img/backend/ktx.hpp"


namespace {

    namespace fs = dal::fs;

    std::optional<fs::path> find_root_path() {
        return dal::find_parent_path_that_has(".git");
    }

}  // namespace


int main(int argc, char** argv) {
    system("chcp 65001");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
