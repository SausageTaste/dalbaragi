#include "work_functions.hpp"

#include <spdlog/fmt/fmt.h>
#include <filesystem>
#include <fstream>

#include <yaml-cpp/yaml.h>


namespace fs = std::filesystem;


namespace dal {

    void work_batch(int argc, char* argv[]) {
        const fs::path path{ "C:\\Users\\AORUS\\Desktop\\monsters.yaml" };
        std::ifstream file{ path };
        auto yaml_data = YAML::Load(file);

        for (const auto& x : yaml_data) {
            const auto name = x["name"].as<std::string>();
            fmt::print("name: {}\n", name);
        }

        return;
    }

}  // namespace dal
