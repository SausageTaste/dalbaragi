#include "work_functions.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define SPDLOG_ACTIVE_LEVEL 0

#include <optional>
#include <unordered_set>

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <sung/basic/byte_arr.hpp>
#include <sung/basic/stringtool.hpp>
#include <sung/basic/threading.hpp>

#include "daltools/bundle/bundle.hpp"
#include "daltools/dmd/exporter.h"
#include "daltools/json/parser.h"
#include "daltools/scene/modifier.h"


namespace fs = std::filesystem;


namespace {

    using byte8 = sung::byte8;


    template <typename... T>
    void throw_fmt(fmt::format_string<T...> fmt, T&&... args) {
        const auto msg = vformat(fmt, fmt::make_format_args(args...));
        SPDLOG_CRITICAL(msg);
        throw std::runtime_error{ msg };
    }

    fs::path resolve_path(const fs::path& path, const fs::path& root) {
        if (path.is_absolute()) {
            return path;
        }

        return root / path;
    }

    bool read_file(const fs::path& path, std::vector<byte8>& out) {
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

    std::optional<std::vector<byte8>> read_file(const fs::path& path) {
        std::vector<byte8> buffer;
        if (!read_file(path, buffer)) {
            return std::nullopt;
        }

        return buffer;
    }

    bool write_file(const fs::path& path, const byte8* data, size_t size) {
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open())
            return false;

        file.write(reinterpret_cast<const char*>(data), size);
        file.close();
        return true;
    }

    bool write_file(const fs::path& path, const std::vector<byte8>& content) {
        return ::write_file(path, content.data(), content.size());
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
                } else {
                    fmt::print("Unknown entry: {}\n", entry_name);
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


    class BundleBuilder {

    public:
        bool add_file(const fs::path& path) {
            const auto name = path.filename().u8string();
            if (added_names_.find(name) != added_names_.end()) {
                return false;
            } else {
                added_names_.insert(name);
            }

            const auto content = ::read_file(path);
            if (!content)
                throw_fmt("Failed to read file: {}\n", path.u8string());

            const auto [offset, size] = data_block_.add_std_arr(*content);
            items_block_.add_nt_str(name.c_str());
            items_block_.add_uint64(offset);
            items_block_.add_uint64(size);

            SPDLOG_INFO("Added '{}' ({})", name, sung::format_bytes(size));
        }

        std::vector<byte8> build(int comp_level) {
            sung::BytesBuilder combined;
            combined.enlarge(sizeof(dal::BundleHeader));

            const auto items_bro = dal::compress_bro(
                items_block_.vector(), comp_level
            );
            const auto items_info = combined.add_std_arr(*items_bro);
            SPDLOG_INFO(
                "Item info: count={}, size={}, size_z={}, ratio={:.2f}",
                added_names_.size(),
                sung::format_bytes(items_block_.size()),
                sung::format_bytes(items_info.second),
                static_cast<double>(items_info.second) / items_block_.size()
            );

            const auto data_bro = dal::compress_bro(
                data_block_.vector(), comp_level
            );
            const auto data_info = combined.add_std_arr(*data_bro);
            SPDLOG_INFO(
                "Data info: size={}, size_z={}, ratio={:.2f}",
                sung::format_bytes(data_block_.size()),
                sung::format_bytes(data_info.second),
                static_cast<double>(data_info.second) / data_block_.size()
            );

            auto& header = *reinterpret_cast<dal::BundleHeader*>(combined.data()
            );
            header.init();
            header.set_items_info(
                items_info.first,
                items_block_.size(),
                items_info.second,
                added_names_.size()
            );
            header.set_data_info(
                data_info.first, data_block_.size(), data_info.second
            );

            return combined.release();
        }

    private:
        sung::BytesBuilder items_block_, data_block_;
        std::unordered_set<std::string> added_names_;
    };


    class JsonTask : public sung::IStandardLoadTask {

    public:
        JsonTask(const WorkDef::Dmd& work_def_, const fs::path& root_path)
            : work_def_(work_def_), root_path_(root_path) {}

        sung::TaskStatus tick() override {
            const auto u8path = fs::u8path(work_def_.path_);
            const auto json_path = ::resolve_path(u8path, root_path_);

            const auto json_data = ::read_file(json_path);
            if (!json_data) {
                return this->fail(
                    "Failed to read file: " + json_path.u8string()
                );
            }

            auto bin_path = json_path;
            bin_path.replace_extension(".bin");

            std::vector<dal::parser::SceneIntermediate> scenes;
            if (const auto bin_data = ::read_file(bin_path))
                dal::parser::parse_json_bin(scenes, *json_data, *bin_data);
            else
                dal::parser::parse_json(scenes, *json_data);

            if (scenes.size() != 1)
                return this->fail_fmt(
                    "Invalid scene count: {}\n", scenes.size()
                );

            for (auto& scene : scenes) {
                for (auto& m : scene.materials_) {
                    textures_in_use_.insert(m.albedo_map_);
                    textures_in_use_.insert(m.normal_map_);
                    textures_in_use_.insert(m.metallic_map_);
                    textures_in_use_.insert(m.roughness_map_);
                }
            }

            scene_ = std::move(scenes[0]);
            return this->success();
        }

        WorkDef::Dmd work_def_;
        dal::parser::SceneIntermediate scene_;
        std::unordered_set<std::string> textures_in_use_;

    private:
        template <typename... T>
        auto fail_fmt(fmt::format_string<T...> fmt, T&&... args) {
            return this->fail(vformat(fmt, fmt::make_format_args(args...)));
        }

        fs::path root_path_;
    };


    class DmdTask : public sung::IStandardLoadTask {

    public:
        DmdTask(
            dal::parser::SceneIntermediate&& scene,
            const WorkDef::Dmd& work_def,
            const fs::path& interm_path
        )
            : scene_(std::move(scene))
            , work_def_(work_def)
            , interm_path_(interm_path) {}

        sung::TaskStatus tick() override {
            dal::parser::flip_uv_vertically(scene_);
            dal::parser::clear_collection_info(scene_);
            dal::parser::reduce_indexed_vertices(scene_);
            dal::parser::remove_duplicate_materials(scene_);
            dal::parser::merge_redundant_mesh_actors(scene_);
            dal::parser::remove_empty_meshes(scene_);
            dal::parser::reduce_joints(scene_);
            dal::parser::apply_root_transform(scene_);

            const auto model = dal::parser::convert_to_model_dmd(scene_);
            const auto bin_built = dal::parser::build_binary_model(
                model, deduce_comp_method(work_def_.comp_method_)
            );

            const auto u8path = fs::u8path(work_def_.path_);
            auto output_path = interm_path_ / u8path.filename();
            output_path.replace_extension(".dmd");
            if (!::write_file(output_path, *bin_built))
                return this->fail(
                    "Failed to write file: " + output_path.u8string()
                );

            return this->success();
        }

    private:
        dal::parser::SceneIntermediate scene_;
        WorkDef::Dmd work_def_;
        fs::path interm_path_;
    };


    class TextureTask : public sung::IStandardLoadTask {

    public:
        TextureTask(
            const WorkDef::Texture& work_def,
            const fs::path& file_path,
            const fs::path& interm_path
        )
            : work_def_(work_def)
            , file_path_(file_path)
            , interm_path_(interm_path) {}

        sung::TaskStatus tick() override {
            const auto ktx_dir = interm_path_.parent_path() / "ktx";
            output_path_ = ktx_dir / file_path_.filename();
            output_path_.replace_extension(".ktx");
            if (fs::is_regular_file(output_path_)) {
                SPDLOG_DEBUG("Use existing KTX: {}", output_path_.u8string());
                return this->success();
            }

            fs::path src_path;
            if (!this->is_ktx_compatible(file_path_)) {
                const auto png_dir = interm_path_.parent_path() / "png";
                src_path = png_dir / file_path_.filename();
                src_path.replace_extension(".png");

                if (fs::exists(src_path)) {
                    SPDLOG_DEBUG("Use existing PNG: {}", src_path.u8string());
                } else {
                    const auto content = ::read_file(file_path_);
                    if (!content)
                        return this->fail("Failed to read image file");

                    int width, height, channels;
                    const auto img = stbi_load_from_memory(
                        content->data(),
                        content->size(),
                        &width,
                        &height,
                        &channels,
                        0
                    );

                    ImageWriteContext ctx;
                    const auto res = stbi_write_png_to_func(
                        write_img,
                        &ctx,
                        width,
                        height,
                        channels,
                        img,
                        width * channels
                    );
                    if (0 == res)
                        return this->fail("Failed to write PNG file");

                    stbi_image_free(img);

                    fs::create_directories(png_dir);
                    if (!::write_file(src_path, ctx.data_))
                        return this->fail("Failed to write PNG file");
                }
            } else {
                src_path = file_path_;
            }

            if (!fs::is_regular_file(src_path))
                return this->fail("Source file not found");

            std::string ktx_cmd;
            ktx_cmd += "ktx create --encode uastc --generate-mipmap";

            std::string format_prefix;
            if (work_def_.channel_ == "RGBA")
                format_prefix = "R8G8B8A8";
            else if (work_def_.channel_ == "RGB")
                format_prefix = "R8G8B8";
            else if (work_def_.channel_ == "RG")
                format_prefix = "R8G8";
            else if (work_def_.channel_ == "R")
                format_prefix = "R8";
            else
                return this->fail_fmt(
                    "Invalid channel ({})", work_def_.channel_
                );

            if (work_def_.srgb_) {
                ktx_cmd += fmt::format(" --format {}_SRGB", format_prefix);
                ktx_cmd += " --assign-oetf srgb";
                ktx_cmd += " --convert-oetf srgb";
            } else {
                ktx_cmd += fmt::format(" --format {}_UNORM", format_prefix);
                ktx_cmd += " --assign-oetf linear";
                ktx_cmd += " --convert-oetf linear";
            }

            ktx_cmd += ' ';
            ktx_cmd += '"';
            ktx_cmd += src_path.u8string();
            ktx_cmd += '"';

            ktx_cmd += ' ';
            ktx_cmd += '"';
            ktx_cmd += output_path_.u8string();
            ktx_cmd += '"';

            SPDLOG_DEBUG("KTX command: {}", ktx_cmd);

            fs::create_directories(ktx_dir);
            if (0 != system(ktx_cmd.c_str()))
                return this->fail("Failed KTX command");

            return this->success();
        }

        fs::path output_path_;

    private:
        struct ImageWriteContext {
            std::vector<byte8> data_;
        };

        template <typename... T>
        auto fail_fmt(fmt::format_string<T...> fmt, T&&... args) {
            return this->fail(vformat(fmt, fmt::make_format_args(args...)));
        }

        static void write_img(void* context, void* data, int size) {
            auto& ctx = *reinterpret_cast<ImageWriteContext*>(context);
            ctx.data_.insert(
                ctx.data_.end(), (byte8*)data, (byte8*)data + size
            );
        }

        static bool is_ktx_compatible(const fs::path& path) {
            const auto ext = path.extension().u8string();
            return ext == ".png";
        }

        WorkDef::Texture work_def_;
        fs::path file_path_;
        fs::path interm_path_;
    };

}  // namespace


namespace dal {

    void work_batch(int argc, char* argv[]) {
        spdlog::set_level(spdlog::level::debug);

        if (argc < 3) {
            fmt::print("Usage: dalbatch <path>\n");
            return;
        }

        auto task_sche = sung::create_task_scheduler();

        const fs::path yam_path = fs::u8path(argv[2]);
        std::ifstream file{ yam_path };
        const auto yam = YAML::Load(file);

        ::WorkDef work;
        work.parse(yam);
        // work.print_all();

        const auto root_path = yam_path.parent_path();
        const auto out_path = root_path / "out";
        const auto interm_path = out_path / "intermediate";
        const auto final_path = out_path / "final";

        if (fs::is_directory(final_path) && !fs::exists(interm_path)) {
            fs::rename(final_path, interm_path);
        }

        std::vector<std::shared_ptr<::JsonTask>> json_tasks;
        for (auto dmd_work : work.dmd()) {
            auto& added = json_tasks.emplace_back();
            added = std::make_shared<::JsonTask>(dmd_work, root_path);
            task_sche->add_task(added);
        }

        std::unordered_set<std::string> textures_in_use;
        std::vector<std::shared_ptr<sung::IStandardLoadTask>> fire_tasks;
        for (auto& json_task : json_tasks) {
            json_task->wait_spinlock();
            textures_in_use.merge(json_task->textures_in_use_);

            auto& added = fire_tasks.emplace_back();
            added = std::make_shared<::DmdTask>(
                std::move(json_task->scene_), json_task->work_def_, interm_path
            );
            task_sche->add_task(added);
        }

        textures_in_use.erase("");
        std::unordered_set<std::string> textures_copied;

        std::vector<std::shared_ptr<::TextureTask>> tex_tasks;
        for (const auto& tex : work.tex()) {
            if (textures_in_use.find(tex.name_) == textures_in_use.end())
                continue;

            const auto src_path = work.find_tex_file(tex.name_, root_path);
            auto& added = tex_tasks.emplace_back();
            added = std::make_shared<::TextureTask>(tex, src_path, interm_path);
            task_sche->add_task(added);
        }

        std::vector<fs::path> tex_ready;
        for (const auto& tex : textures_in_use) {
            if (textures_copied.find(tex) != textures_copied.end())
                continue;

            const auto src_path = work.find_tex_file(tex, root_path);
            if (!fs::is_regular_file(src_path))
                throw_fmt("Failed to copy texture: {}\n", src_path.u8string());

            tex_ready.push_back(src_path);
        }

        for (auto& task : tex_tasks) {
            task->wait_spinlock();
            if (task->has_failed())
                throw_fmt("Texture task failed: {}\n", task->err_msg());

            tex_ready.push_back(task->output_path_);
        }

        for (auto& task : fire_tasks) {
            task->wait_spinlock();
            if (task->has_failed())
                throw_fmt("A task failed: {}\n", task->err_msg());
        }

        if (work.bundle()) {
            ::BundleBuilder dun_builder;

            for (const auto& path : tex_ready) {
                dun_builder.add_file(path);
            }

            for (const auto& dmd : work.dmd()) {
                const auto u8path = fs::u8path(dmd.path_);
                auto path = interm_path / u8path.filename();
                path.replace_extension(".dmd");
                dun_builder.add_file(path);
            }

            const auto comp_level = work.bundle()->comp_level_;
            const auto bin_data = dun_builder.build(comp_level);
            const auto dun_path = final_path / work.bundle()->name_;
            ::write_file(dun_path, bin_data);

            SPDLOG_INFO(
                "Output: '{}' ({})",
                dun_path.u8string(),
                sung::format_bytes(bin_data.size())
            );
        } else {
            fs::rename(interm_path, final_path);
        }

        return;
    }

}  // namespace dal
