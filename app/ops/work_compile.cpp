#include "work_functions.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <set>

#include <spdlog/fmt/fmt.h>
#include <argparse/argparse.hpp>

#include "dal/auxiliary/path.hpp"
#include "dal/common/konst.hpp"
#include "dal/dmd/parser.hpp"
#include "dal/dmd/exporter.hpp"
#include "dal/json/parser.hpp"
#include "dal/scene/modifier.hpp"


namespace {

    std::optional<std::vector<uint8_t>> read_file(
        const std::filesystem::path& path
    ) {
        std::ifstream file{ path,
                            std::ios::ate | std::ios::binary | std::ios::in };

        if (!file.is_open())
            return std::nullopt;

        const auto file_size = static_cast<size_t>(file.tellg());
        std::vector<uint8_t> buffer;
        buffer.resize(file_size);

        file.seekg(0);
        file.read((char*)buffer.data(), buffer.size());
        file.close();

        return buffer;
    }

    dal::CompressMethod interpret_comp_method(const std::string& str) {
        const std::map<dal::CompressMethod, std::set<std::string>> map{
            { dal::CompressMethod::none, { "none", "0" } },
            { dal::CompressMethod::zip, { "zip", "1" } },
            { dal::CompressMethod::brotli, { "brotli", "2" } },
        };

        for (const auto& [method, strs] : map) {
            if (strs.find(str) != strs.end()) {
                return method;
            }
        }

        throw std::runtime_error{ "Invalid compression method: " + str };
    }

    void do_file(
        const std::filesystem::path& src_path, dal::CompressMethod comp_method
    ) {
        using namespace dal;

        const auto json_data = ::read_file(src_path);

        auto bin_path = src_path;
        bin_path.replace_extension("bin");

        std::vector<SceneIntermediate> scenes;
        JsonParseResult result;
        if (const auto bin_data = ::read_file(bin_path)) {
            result = parse_json_bin(scenes, *json_data, *bin_data);
        } else {
            result = parse_json(scenes, *json_data);
        }

        std::vector<std::string> tex_lookup_paths{ dal::tostr(src_path) };

        for (auto& scene : scenes) {
            for (auto& mesh : scene.meshes_) {
                flip_uv_vertically(mesh);
            }
            clear_collection_info(scene);
            optimize_scene(scene, tex_lookup_paths);
        }

        const auto model = convert_to_model_dmd(scenes.at(0));
        const auto bin_built = build_binary_model(model, comp_method);

        std::filesystem::path output_path = src_path;
        output_path.replace_extension("dmd");

        std::ofstream file(output_path, std::ios::binary);
        file.write((const char*)bin_built->data(), bin_built->size());
        file.close();
    }

}  // namespace


namespace dal {

    void work_compile(int argc, char* argv[]) {
        argparse::ArgumentParser parser{ "daltools" };
        parser.add_argument("operation").help("Operation name");
        parser.add_argument("-c", "--compress")
            .help("Select compression method (0: none, 1: zip, 2: brotli)")
            .default_value("2");
        parser.add_argument("files").help("Input model file paths").remaining();
        parser.parse_args(argc, argv);

        const auto comp_method = ::interpret_comp_method(
            parser.get<std::string>("--compress")
        );

        const auto files = parser.get<std::vector<std::string>>("files");
        for (const auto& src_path_str : files) {
            const std::filesystem::path src_path{ src_path_str };
            ::do_file(src_path, comp_method);
        }
    }

}  // namespace dal
