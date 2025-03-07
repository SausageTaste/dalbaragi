#include "work_functions.hpp"

#include <optional>
#include <unordered_set>

#include <spdlog/fmt/fmt.h>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <sung/basic/bytes.hpp>

#include "daltools/dmd/exporter.h"
#include "daltools/json/parser.h"
#include "daltools/scene/modifier.h"


namespace fs = std::filesystem;


namespace {

    template <typename... T>
    void throw_fmt(fmt::format_string<T...> fmt, T&&... args) {
        throw std::runtime_error{
            vformat(fmt, fmt::make_format_args(args...))
        };
    }

    fs::path resolve_path(const fs::path& path, const fs::path& root) {
        if (path.is_absolute()) {
            return path;
        }

        return root / path;
    }

    bool read_file(const fs::path& path, std::vector<sung::byte8>& out) {
        std::ifstream file{ path,
                            std::ios::ate | std::ios::binary | std::ios::in };

        if (!file.is_open()) {
            return false;
        }

        const auto file_size = static_cast<size_t>(file.tellg());
        out.resize(file_size);

        file.seekg(0);
        file.read((char*)out.data(), out.size());
        file.close();

        return true;
    }

    std::optional<std::vector<sung::byte8>> read_file(const fs::path& path) {
        std::vector<sung::byte8> buffer;
        if (!read_file(path, buffer)) {
            return std::nullopt;
        }

        return buffer;
    }

    dal::CompressMethod deduce_comp_method(const std::string& str) {
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

        throw_fmt("Invalid compression method: {}\n", str);
    }


    class WorkDef {

    public:
        struct Dmd {
            std::string path_;
            std::string comp_method_;
        };

        struct Bundle {
            std::string name_;
            int comp_level_ = 0;
        };

        struct Texture {
            std::string name_;
            std::string channel_;
            bool srgb_ = false;
        };

        // Returns error message if failed, otherwise empty string
        void parse(const YAML::Node& yam) {
            for (auto x : yam) {
                const auto entry_name = x.first.as<std::string>();
                const auto& data = x.second;

                if (entry_name._Starts_with("dmd")) {
                    auto& dst = dmd_.emplace_back();
                    dst.path_ = data["path"].as<std::string>();
                    dst.comp_method_ =
                        data["compression_method"].as<std::string>();
                } else if (entry_name._Starts_with("bundle")) {
                    if (bundle_) {
                        throw_fmt("Only one bundle is allowed.");
                    } else {
                        bundle_ = Bundle{};
                        auto& dst = bundle_.value();
                        dst.name_ = data["name"].as<std::string>();
                        dst.comp_level_ = data["compression_level"].as<int>();
                    }
                } else if (entry_name._Starts_with("texture_lookup_paths")) {
                    for (auto& s : data) {
                        texture_lookup_paths_.emplace_back(s.as<std::string>());
                    }
                } else if (entry_name._Starts_with("texture_list")) {
                    this->parse_texture_list(data);
                }
            }
        }

        fs::path find_tex_file(const std::string& src, const fs::path& root) {
            for (const auto& lup : texture_lookup_paths_) {
                const auto lup_resolved = resolve_path(lup, root);
                if (!fs::is_directory(lup_resolved)) {
                    throw_fmt(
                        "Texture lookup path is not a directory: {}\n", lup
                    );
                }

                const auto tex_path = lup_resolved / src;
                if (fs::exists(tex_path)) {
                    return tex_path;
                }
            }

            {
                const auto tex_path = root / src;
                if (fs::exists(tex_path)) {
                    return tex_path;
                }
            }

            {
                const auto tex_path = fs::u8path(src);
                if (fs::exists(tex_path)) {
                    return tex_path;
                }
            }

            throw_fmt("Texture not found: {}\n", src);
        }

        void print_all() const {
            for (const auto& tex : textures_) {
                fmt::print(
                    "Texture: name={}, channel={}, srgb={}\n",
                    tex.name_,
                    tex.channel_,
                    tex.srgb_
                );
            }
            for (const auto& path : texture_lookup_paths_) {
                fmt::print("Texture lookup path: {}\n", path);
            }
            for (const auto& dmd : dmd_) {
                fmt::print(
                    "DMD: path={}, comp={}\n", dmd.path_, dmd.comp_method_
                );
            }
            if (bundle_) {
                fmt::print(
                    "Bundle: name={}, comp_level={}\n",
                    bundle_->name_,
                    bundle_->comp_level_
                );
            }
        }

        auto& dmd() const { return dmd_; }
        auto& tex() const { return textures_; }
        auto& tex_lookup_paths() const { return texture_lookup_paths_; }
        auto& bundle() const { return bundle_; }

    private:
        void parse_texture_list(const YAML::Node& yam) {
            for (auto& entry : yam) {
                const auto channels = entry["channels"].as<std::string>();
                const auto srgb = entry["srgb"].as<bool>();

                for (auto& s : entry["src"]) {
                    auto& dst = textures_.emplace_back();
                    dst.name_ = s.as<std::string>();
                    dst.channel_ = channels;
                    dst.srgb_ = srgb;
                }
            }
        }

        std::vector<Dmd> dmd_;
        std::vector<Texture> textures_;
        std::vector<std::string> texture_lookup_paths_;
        std::optional<Bundle> bundle_;
    };

}  // namespace


namespace dal {

    void work_batch(int argc, char* argv[]) {
        if (argc < 3) {
            fmt::print("Usage: dalbatch <path>\n");
            return;
        }

        const fs::path yam_path = fs::u8path(argv[2]);
        std::ifstream file{ yam_path };
        auto yam = YAML::Load(file);

        ::WorkDef work;
        work.parse(yam);
        work.print_all();

        const auto root_path = yam_path.parent_path();
        const auto out_path = root_path / "out";
        const auto interm_path = out_path / "intermediate";

        std::unordered_set<std::string> textures_in_use;

        for (auto dmd_work : work.dmd()) {
            const auto u8path = fs::u8path(dmd_work.path_);
            const auto json_path = ::resolve_path(u8path, root_path);

            const auto json_data = ::read_file(json_path);
            if (!json_data) {
                throw_fmt("Failed to read file: {}\n", json_path.u8string());
            }

            auto bin_path = json_path;
            bin_path.replace_extension(".bin");

            std::vector<dal::parser::SceneIntermediate> scenes;
            if (const auto bin_data = ::read_file(bin_path))
                dal::parser::parse_json_bin(scenes, *json_data, *bin_data);
            else
                dal::parser::parse_json(scenes, *json_data);

            for (auto& scene : scenes) {
                for (auto& m : scene.materials_) {
                    textures_in_use.insert(m.albedo_map_);
                    textures_in_use.insert(m.normal_map_);
                    textures_in_use.insert(m.metallic_map_);
                    textures_in_use.insert(m.roughness_map_);
                }

                /*
                dal::parser::flip_uv_vertically(scene);
                dal::parser::clear_collection_info(scene);
                dal::parser::reduce_indexed_vertices(scene);
                dal::parser::remove_duplicate_materials(scene);
                dal::parser::merge_redundant_mesh_actors(scene);
                dal::parser::remove_empty_meshes(scene);
                dal::parser::reduce_joints(scene);
                dal::parser::apply_root_transform(scene);
                */
            }

            const auto model = convert_to_model_dmd(scenes.at(0));
            const auto bin_built = dal::parser::build_binary_model(
                model, deduce_comp_method(dmd_work.comp_method_)
            );

            auto output_path = interm_path / u8path.filename();
            output_path.replace_extension(".dmd");

            fs::create_directories(output_path.parent_path());
            std::ofstream file(
                output_path.u8string().c_str(), std::ios::binary
            );
            file.write((const char*)bin_built->data(), bin_built->size());
            file.close();
        }

        textures_in_use.erase("");
        std::unordered_set<std::string> textures_copied;

        for (const auto& tex : work.tex()) {
            const auto src_path = work.find_tex_file(tex.name_, root_path);
            const auto dst_path = interm_path / src_path.filename();
            fs::copy_file(
                src_path, dst_path, fs::copy_options::update_existing
            );
            if (!fs::is_regular_file(dst_path))
                throw_fmt("Failed to copy texture: {}\n", src_path.u8string());

            textures_copied.insert(tex.name_);
        }

        for (const auto& tex : textures_in_use) {
            if (textures_copied.find(tex) != textures_copied.end())
                continue;

            const auto src_path = work.find_tex_file(tex, root_path);
            const auto dst_path = interm_path / src_path.filename();
            fs::copy_file(
                src_path, dst_path, fs::copy_options::update_existing
            );
            if (!fs::is_regular_file(dst_path))
                throw_fmt("Failed to copy texture: {}\n", src_path.u8string());
        }

        return;
    }

}  // namespace dal
