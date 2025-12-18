#include "work_functions.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define SPDLOG_ACTIVE_LEVEL 0

#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <unordered_set>

#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <yaml-cpp/yaml.h>
#include <sung/basic/byte_arr.hpp>
#include <sung/basic/stringtool.hpp>
#include <sung/basic/time.hpp>

#include "dal/auxiliary/err_msg_holder.hpp"
#include "dal/auxiliary/path.hpp"
#include "dal/bundle/bundle.hpp"
#include "dal/common/task_sys.hpp"
#include "dal/dmd/exporter.hpp"
#include "dal/json/parser.hpp"
#include "dal/scene/mesh_opt.hpp"
#include "dal/scene/modifier.hpp"


#define THROWF(...)                                \
    do {                                           \
        const auto msg = fmt::format(__VA_ARGS__); \
        SPDLOG_CRITICAL(msg);                      \
        throw std::runtime_error{ msg };           \
    } while (0)


namespace fs = std::filesystem;


namespace fmt {

    template <>
    struct fmt::formatter<std::filesystem::path>
        : fmt::formatter<std::string_view> {
        template <typename FormatContext>
        auto format(const std::filesystem::path& p, FormatContext& ctx) const {
            return fmt::formatter<std::string_view>::format(dal::tostr(p), ctx);
        }
    };

}  // namespace fmt


namespace {

    using byte8 = sung::byte8;


    fs::path find_yml_path(int argc, char* argv[]) {
        if (argc >= 3) {
            return dal::u8path(argv[2]);
        } else if (argc >= 2) {
            return "dalbatch.yml";
        } else {
            THROWF("No YAML file specified");
        }
    }

    std::string clean_tex_path(std::string path) {
        if (path._Starts_with("//")) {
            path = path.substr(2);
        } else if (path._Starts_with("/")) {
            path = path.substr(1);
        }

        return path;
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

        THROWF("Invalid compression method: {}\n", str);
        return dal::CompressMethod::none;
    }

    std::string replace_ext(const std::string& str, const std::string& ext) {
        const auto pos = str.find_last_of('.');
        if (pos == std::string::npos) {
            return str + '.' + ext;
        } else {
            return str.substr(0, pos) + '.' + ext;
        }
    }

    bool match_pattern(
        const std::string& filename, const std::string& pattern
    ) {
        size_t f = 0, p = 0;
        size_t starPos = std::string::npos, match = 0;

        while (f < filename.size()) {
            if (p < pattern.size() &&
                (pattern[p] == filename[f] || pattern[p] == '?')) {
                ++p;
                ++f;
            } else if (p < pattern.size() && pattern[p] == '*') {
                starPos = p++;
                match = f;
            } else if (starPos != std::string::npos) {
                p = starPos + 1;
                f = ++match;
            } else {
                return false;
            }
        }

        while (p < pattern.size() && pattern[p] == '*') ++p;
        return p == pattern.size();
    }


    class TimerLogger {

    public:
        void log(const char* text) {
            SPDLOG_INFO("{} ({:.3f} sec)", text, timer_.check_get_elapsed());
        }

    private:
        sung::MonotonicRealtimeTimer timer_;
    };


    class WorkDef {

    public:
        struct Dmd {
            std::string path_;
            std::string comp_method_;
            bool detect_alpha_ = false;
            bool merge_vertex_dups_ = false;
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
                    this->parse_dmd(data);
                } else if (entry_name._Starts_with("bundle")) {
                    if (bundle_) {
                        THROWF("Only one bundle is allowed.");
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

        void notify_root(const fs::path& root) {
            texture_lookup_paths_.emplace_back(dal::tostr(root));
        }

        fs::path find_tex_file(
            const std::string& src, const fs::path& root
        ) const {
            for (const auto& lup : texture_lookup_paths_) {
                if (lup.empty())
                    continue;

                const auto lup_resolved = resolve_path(lup, root);
                if (!fs::is_directory(lup_resolved)) {
                    THROWF("Texture lookup path is not a directory: {}\n", lup);
                }

                const auto tex_path = lup_resolved / dal::u8path(src);
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
                const auto tex_path = dal::u8path(src);
                if (fs::exists(tex_path)) {
                    return tex_path;
                }
            }

            THROWF("Texture not found: {}\n", src);
            return {};
        }

        const Texture* find_tex_entry(std::string name) const {
            name = dal::tostr(dal::u8path(name).filename());

            for (const auto& tex : textures_) {
                if (::match_pattern(name, tex.name_)) {
                    return &tex;
                }
            }

            return nullptr;
        }

        bool has_tex(std::string name) const {
            name = dal::tostr(dal::u8path(name).filename());

            for (const auto& tex : textures_) {
                if (::match_pattern(name, tex.name_)) {
                    return true;
                }
            }

            return false;
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
        // Texture lookup paths
        auto& lup() const { return texture_lookup_paths_; }
        auto& bundle() const { return bundle_; }

    private:
        bool get_bool(const YAML::Node& yam, const std::string& key) {
            try {
                return yam[key].as<bool>();
            } catch (const std::runtime_error&) {
                return false;
            }
        }

        void parse_dmd(const YAML::Node& yam) {
            auto& dst = dmd_.emplace_back();
            dst.path_ = yam["path"].as<std::string>();
            dst.comp_method_ = yam["compression_method"].as<std::string>();
            dst.detect_alpha_ = this->get_bool(yam, "detect_alpha");
            dst.merge_vertex_dups_ = this->get_bool(yam, "merge_vertex_dups");
        }

        void parse_texture_list(const YAML::Node& yam) {
            for (auto& entry : yam) {
                const auto channels = entry["channels"].as<std::string>();
                const auto srgb = entry["srgb"].as<bool>();

                for (auto& s : entry["src"]) {
                    auto name = s.as<std::string>();
                    if (name.empty())
                        THROWF("Empty texture name");
                    if (this->has_tex(name))
                        THROWF("Duplicated texture name: {}", name);

                    auto& dst = textures_.emplace_back();
                    dst.name_ = std::move(name);
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


    class FileList {

    public:
        void insert(const fs::path& path) {
            const auto name = dal::tostr(path.filename());
            if (data_.find(name) != data_.end()) {
                THROWF("Duplicated file name: {}\n", name);
            }

            data_[name] = path;
        }

        auto begin() const { return data_.begin(); }
        auto end() const { return data_.end(); }

    private:
        std::map<std::string, fs::path> data_;
    };


    class BundleBuilder {

    public:
        void reserve_data(size_t size) { data_block_.reserve(size); }

        bool add_data(const std::string& name, const std::vector<byte8>& data) {
            added_names_.insert(name);

            const auto [offset, size] = data_block_.add_std_arr(data);
            items_block_.add_nt_str(name.c_str());
            items_block_.add_uint64(offset);
            items_block_.add_uint64(size);

            SPDLOG_INFO("Added '{}' ({})", name, sung::format_bytes(size));
            return true;
        }

        bool add_file(const fs::path& path) {
            const auto name = dal::tostr(path.filename());
            if (added_names_.find(name) != added_names_.end()) {
                return false;
            } else {
                added_names_.insert(name);
            }

            const auto content = ::read_file(path);
            if (!content)
                THROWF("Failed to read file: {}\n", dal::tostr(path));

            return this->add_data(name, *content);
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

            auto& header = *reinterpret_cast<dal::BundleHeader*>(
                combined.data()
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


    class Paths {

    public:
        explicit Paths(const fs::path& yam_path) { yam_path_ = yam_path; }

        void set(const fs::path& yam_path) { yam_path_ = yam_path; }

        fs::path yam() const { return yam_path_; }
        fs::path root() const { return yam_path_.parent_path(); }
        fs::path out() const { return this->root() / "out"; }
        fs::path final() const { return this->out() / "final"; }

    private:
        fs::path yam_path_;
    };

}  // namespace


// Tasks
namespace {

    class YamlTask
        : public enki::TaskSet
        , public dal::ErrorMsgHolder {

    public:
        YamlTask(const Paths& paths) : paths_(paths) {}

        void ExecuteRange(
            enki::TaskSetPartition range_, uint32_t threadnum_
        ) override {
            const auto path_str = dal::tostr(paths_.yam());

            std::ifstream file{ paths_.yam() };
            if (!file.is_open())
                return this->fail("Failed to open file: " + path_str);

            const auto yam = YAML::Load(file);
            if (!yam.IsDefined())
                return this->fail("Failed to parse YAML file: " + path_str);

            work_.parse(yam);
            work_.notify_root(paths_.root());
            // work_.print_all();
        }

        const ::WorkDef& work() const { return work_; }

    private:
        const Paths& paths_;
        ::WorkDef work_;
    };


    class JsonTask
        : public enki::TaskSet
        , public dal::ErrorMsgHolder {

    public:
        JsonTask() = default;
        JsonTask(const JsonTask&) = delete;
        JsonTask& operator=(const JsonTask&) = delete;

        JsonTask(JsonTask&& other) noexcept {
            std::swap(dmd_def_, other.dmd_def_);
            std::swap(paths_, other.paths_);
            std::swap(textures_in_use_, other.textures_in_use_);
            std::swap(scene_, other.scene_);
        }

        JsonTask& operator=(JsonTask&& other) {
            std::swap(dmd_def_, other.dmd_def_);
            std::swap(paths_, other.paths_);
            std::swap(textures_in_use_, other.textures_in_use_);
            std::swap(scene_, other.scene_);
            return *this;
        }

        void init(const WorkDef::Dmd& dmd_def, const Paths& paths) {
            dmd_def_ = &dmd_def;
            paths_ = &paths;
        }

        void ExecuteRange(
            enki::TaskSetPartition range_, uint32_t threadnum_
        ) override {
            const auto u8path = dal::u8path(dmd_def_->path_);
            const auto json_path = ::resolve_path(u8path, paths_->root());

            const auto json_data = ::read_file(json_path);
            if (!json_data) {
                return this->fail("Failed to read file '{}'", json_path);
            }

            auto bin_path = json_path;
            bin_path.replace_extension(".bin");

            std::vector<dal::SceneIntermediate> scenes;
            if (const auto bin_data = ::read_file(bin_path))
                dal::parse_json_bin(scenes, *json_data, *bin_data);
            else
                dal::parse_json(scenes, *json_data);

            if (scenes.size() != 1)
                return this->fail("Invalid scene count: {}\n", scenes.size());

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

        std::unordered_set<std::string>& tex_in_use() {
            return textures_in_use_;
        }
        auto& scene() { return scene_; }
        auto& dmd_def() const { return *dmd_def_; }

    private:
        const WorkDef::Dmd* dmd_def_;
        const Paths* paths_;
        std::unordered_set<std::string> textures_in_use_;
        dal::SceneIntermediate scene_;
    };


    class DmdTask
        : public enki::TaskSet
        , public dal::ErrorMsgHolder {

    public:
        DmdTask() = default;
        DmdTask(const DmdTask&) = delete;
        DmdTask& operator=(const DmdTask&) = delete;

        DmdTask(DmdTask&& other) noexcept {
            std::swap(scene_, other.scene_);
            std::swap(work_def_, other.work_def_);
            std::swap(dmd_def_, other.dmd_def_);
            std::swap(out_dir_, other.out_dir_);
            std::swap(output_path_, other.output_path_);
        }

        DmdTask& operator=(DmdTask&& other) {
            std::swap(scene_, other.scene_);
            std::swap(work_def_, other.work_def_);
            std::swap(dmd_def_, other.dmd_def_);
            std::swap(out_dir_, other.out_dir_);
            std::swap(output_path_, other.output_path_);
            return *this;
        }

        void init(
            dal::SceneIntermediate&& scene,
            const WorkDef& work_def,
            const WorkDef::Dmd& dmd_def,
            const fs::path& out_dir
        ) {
            scene_ = std::move(scene);
            work_def_ = &work_def;
            dmd_def_ = &dmd_def;
            out_dir_ = out_dir;
        }

        void ExecuteRange(
            enki::TaskSetPartition range_, uint32_t threadnum_
        ) override {
            TimerLogger timer;

            dal::clear_collection_info(scene_);
            timer.log("DMD Clear collection info");

            dal::remove_duplicate_materials(scene_);
            timer.log("DMD Remove duplicate materials");
            dal::merge_redundant_mesh_actors(scene_);
            timer.log("DMD Merge redundant mesh actors");

            if (dmd_def_->detect_alpha_) {
                dal::split_by_transparency(scene_, work_def_->lup());
                timer.log("DMD Split by transparency");
            }

            dal::remove_empty_meshes(scene_);
            timer.log("DMD Remove empty meshes");
            dal::reduce_joints(scene_);
            timer.log("DMD Reduce joints");
            dal::apply_root_transform(scene_);
            timer.log("DMD Apply root transform");

            for (auto& mesh : scene_.meshes_) {
                if (dmd_def_->merge_vertex_dups_)
                    dal::reduce_indexed_vertices(mesh);

                dal::flip_uv_vertically(mesh);
                dal::optimize_vertex_cache(mesh);
                dal::optimize_vertex_fetch(mesh);
            }
            timer.log("DMD Per mesh operations");

            for (auto& m : scene_.materials_) {
                clean_tex_paths(m);
            }
            timer.log("DMD Update texture paths");

            const auto model = dal::convert_to_model_dmd(scene_);
            timer.log("DMD Build model");
            const auto bin_built = dal::build_binary_model(
                model, deduce_comp_method(dmd_def_->comp_method_)
            );
            timer.log("DMD Build binary data");

            const auto u8path = dal::u8path(dmd_def_->path_);
            output_path_ = out_dir_ / "dmd" / u8path.filename();
            output_path_.replace_extension(".dmd");
            if (!::write_file(output_path_, *bin_built))
                return this->fail(
                    "Failed to write file: " + dal::tostr(output_path_)
                );

            return this->success();
        }

        const fs::path& output_path() const { return output_path_; }

    private:
        void clean_tex_paths(dal::SceneIntermediate::Material& m) {
            m.albedo_map_ = ::clean_tex_path(m.albedo_map_);
            m.normal_map_ = ::clean_tex_path(m.normal_map_);
            m.metallic_map_ = ::clean_tex_path(m.metallic_map_);
            m.roughness_map_ = ::clean_tex_path(m.roughness_map_);

            if (work_def_->has_tex(m.albedo_map_))
                m.albedo_map_ = ::replace_ext(m.albedo_map_, "ktx");
            if (work_def_->has_tex(m.normal_map_))
                m.normal_map_ = ::replace_ext(m.normal_map_, "ktx");
            if (work_def_->has_tex(m.metallic_map_))
                m.metallic_map_ = ::replace_ext(m.metallic_map_, "ktx");
            if (work_def_->has_tex(m.roughness_map_))
                m.roughness_map_ = ::replace_ext(m.roughness_map_, "ktx");

            m.albedo_map_ = dal::tostr(dal::u8path(m.albedo_map_).filename());
            m.normal_map_ = dal::tostr(dal::u8path(m.normal_map_).filename());
            m.metallic_map_ = dal::tostr(
                dal::u8path(m.metallic_map_).filename()
            );
            m.roughness_map_ = dal::tostr(
                dal::u8path(m.roughness_map_).filename()
            );
        }

        const WorkDef* work_def_;
        const WorkDef::Dmd* dmd_def_;
        dal::SceneIntermediate scene_;
        fs::path out_dir_;
        fs::path output_path_;
    };


    class TextureTask
        : public enki::TaskSet
        , public dal::ErrorMsgHolder {

    public:
        TextureTask() = default;
        TextureTask(const TextureTask&) = delete;
        TextureTask& operator=(const TextureTask&) = delete;

        TextureTask(TextureTask&& other) noexcept {
            std::swap(work_def_, other.work_def_);
            std::swap(file_path_, other.file_path_);
            std::swap(out_dir_, other.out_dir_);
            std::swap(output_path_, other.output_path_);
        }

        TextureTask& operator=(TextureTask&& other) {
            std::swap(work_def_, other.work_def_);
            std::swap(file_path_, other.file_path_);
            std::swap(out_dir_, other.out_dir_);
            std::swap(output_path_, other.output_path_);
            return *this;
        }

        void init(
            const WorkDef::Texture& work_def,
            const fs::path& file_path,
            const fs::path& out_dir
        ) {
            work_def_ = &work_def;
            file_path_ = file_path;
            out_dir_ = out_dir;
        }

        void ExecuteRange(
            enki::TaskSetPartition range_, uint32_t threadnum_
        ) override {
            const auto ktx_dir = out_dir_ / "ktx";
            output_path_ = ktx_dir / file_path_.filename();
            output_path_.replace_extension(".ktx");
            if (fs::is_regular_file(output_path_)) {
                SPDLOG_DEBUG("Use existing KTX: {}", dal::tostr(output_path_));
                return this->success();
            }

            fs::path src_path;
            if (!this->is_ktx_compatible(file_path_)) {
                const auto png_dir = out_dir_ / "png";
                src_path = png_dir / file_path_.filename();
                src_path.replace_extension(".png");

                if (fs::exists(src_path)) {
                    SPDLOG_DEBUG("Use existing PNG: {}", dal::tostr(src_path));
                } else {
                    const auto content = ::read_file(file_path_);
                    if (!content)
                        return this->fail("Failed to read image file");

                    int width, height, channels;
                    const auto img = stbi_load_from_memory(
                        content->data(),
                        static_cast<int>(content->size()),
                        &width,
                        &height,
                        &channels,
                        0
                    );
                    if (!img)
                        return this->fail("Failed to load image");

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
            if (work_def_->channel_ == "RGBA")
                format_prefix = "R8G8B8A8";
            else if (work_def_->channel_ == "RGB")
                format_prefix = "R8G8B8";
            else if (work_def_->channel_ == "RG")
                format_prefix = "R8G8";
            else if (work_def_->channel_ == "R")
                format_prefix = "R8";
            else
                return this->fail("Invalid channel ({})", work_def_->channel_);

            if (work_def_->srgb_) {
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
            ktx_cmd += dal::tostr(src_path);
            ktx_cmd += '"';

            ktx_cmd += ' ';
            ktx_cmd += '"';
            ktx_cmd += dal::tostr(output_path_);
            ktx_cmd += '"';

            SPDLOG_DEBUG("KTX command: {}", ktx_cmd);

            fs::create_directories(ktx_dir);
            if (0 != system(ktx_cmd.c_str()))
                return this->fail("Failed KTX '{}'", src_path);

            return this->success();
        }

        const fs::path& output_path() const { return output_path_; }

    private:
        struct ImageWriteContext {
            std::vector<byte8> data_;
        };

        static void write_img(void* context, void* data, int size) {
            auto& ctx = *reinterpret_cast<ImageWriteContext*>(context);
            ctx.data_.insert(
                ctx.data_.end(), (byte8*)data, (byte8*)data + size
            );
        }

        static bool is_ktx_compatible(const fs::path& path) {
            const auto ext = dal::tostr(path.extension());
            return ext == ".png";
        }

        const WorkDef::Texture* work_def_;
        fs::path file_path_;
        fs::path out_dir_;
        fs::path output_path_;
    };


    class FileLoadTask
        : public enki::TaskSet
        , public dal::ErrorMsgHolder {

    public:
        FileLoadTask() = default;
        FileLoadTask(const FileLoadTask&) = delete;
        FileLoadTask& operator=(const FileLoadTask&) = delete;

        FileLoadTask(FileLoadTask&& other) noexcept {
            std::swap(file_path_, other.file_path_);
            std::swap(data_, other.data_);
        }

        FileLoadTask& operator=(FileLoadTask&& other) {
            std::swap(file_path_, other.file_path_);
            std::swap(data_, other.data_);
            return *this;
        }

        void init(const fs::path& file_path) { file_path_ = file_path; }

        void ExecuteRange(
            enki::TaskSetPartition range_, uint32_t threadnum_
        ) override {
            if (!::read_file(file_path_, data_))
                return this->fail("Failed to read file: {}\n", file_path_);

            return this->success();
        }

        void free_mem() {
            data_.clear();
            data_.shrink_to_fit();
        }

        auto& path() const { return file_path_; }
        auto& data() const { return data_; }
        auto size() const { return data_.size(); }
        auto name() const { return dal::tostr(file_path_.filename()); }

    private:
        fs::path file_path_;
        std::vector<byte8> data_;
    };

}  // namespace


namespace dal {

    void work_batch(int argc, char* argv[]) {
        spdlog::set_level(spdlog::level::debug);

        ::Paths paths{ find_yml_path(argc, argv) };
        ::YamlTask yam_task(paths);

        auto& ts = dal::tasker();
        ts.AddTaskSetToPipe(&yam_task);
        ts.WaitforAll();
        if (yam_task.has_failed())
            THROWF("YAML task failed: {}\n", yam_task.err_msg());

        std::vector<::JsonTask> json_tasks;
        for (auto& dmd : yam_task.work().dmd()) {
            auto& task = json_tasks.emplace_back();
            task.init(dmd, paths);
        }
        for (auto& json_task : json_tasks) {
            ts.AddTaskSetToPipe(&json_task);
        }
        ts.WaitforAll();

        std::unordered_set<std::string> textures_in_use;
        std::vector<::DmdTask> dmd_tasks;
        for (auto& json_task : json_tasks) {
            if (json_task.has_failed())
                THROWF("JSON task failed: {}\n", json_task.err_msg());

            textures_in_use.merge(json_task.tex_in_use());

            auto& dmd_task = dmd_tasks.emplace_back();
            dmd_task.init(
                std::move(json_task.scene()),
                yam_task.work(),
                json_task.dmd_def(),
                paths.out()
            );
        }
        for (auto& dmd_task : dmd_tasks) {
            ts.AddTaskSetToPipe(&dmd_task);
        }
        textures_in_use.erase("");

        std::unordered_set<std::string> textures_copied;
        std::vector<::TextureTask> tex_tasks;
        ::FileList final_files;
        for (auto tex_name : textures_in_use) {
            tex_name = ::clean_tex_path(tex_name);
            const auto src_path = yam_task.work().find_tex_file(
                tex_name, paths.root()
            );
            if (!fs::is_regular_file(src_path))
                THROWF("Texture not found: {}\n", dal::tostr(src_path));

            const auto tex_entry = yam_task.work().find_tex_entry(tex_name);
            if (tex_entry) {
                auto& tex_task = tex_tasks.emplace_back();
                tex_task.init(*tex_entry, src_path, paths.out());
                textures_copied.insert(tex_name);
            } else {
                final_files.insert(src_path);
            }
        }
        for (auto& tex_task : tex_tasks) {
            ts.AddTaskSetToPipe(&tex_task);
        }

        ts.WaitforAll();

        for (auto& task : tex_tasks) {
            if (task.has_failed())
                THROWF("Texture task failed: {}", task.err_msg());
            final_files.insert(task.output_path());
        }

        for (auto& task : dmd_tasks) {
            if (task.has_failed())
                THROWF("DMD task failed: {}", task.err_msg());
            final_files.insert(task.output_path());
        }

        if (yam_task.work().bundle()) {
            std::vector<::FileLoadTask> file_tasks;
            for (const auto& [name, path] : final_files) {
                auto& task = file_tasks.emplace_back();
                task.init(path);
            }
            for (auto& task : file_tasks) {
                ts.AddTaskSetToPipe(&task);
            }
            ts.WaitforAll();

            size_t total_data_size = 0;
            for (const auto& task : file_tasks) {
                if (task.has_failed())
                    THROWF("File load task failed: {}", task.err_msg());
                total_data_size += task.size();
            }

            ::BundleBuilder dun_builder;
            dun_builder.reserve_data(total_data_size);
            for (const auto& task : file_tasks)
                dun_builder.add_data(task.name(), task.data());

            const auto comp_level = yam_task.work().bundle()->comp_level_;
            const auto bin_data = dun_builder.build(comp_level);
            const auto dun_path = paths.final() /
                                  yam_task.work().bundle()->name_;
            ::write_file(dun_path, bin_data);

            SPDLOG_INFO(
                "Output: '{}' ({})",
                dal::tostr(dun_path),
                sung::format_bytes(bin_data.size())
            );
        } else {
            for (const auto& [name, path] : final_files) {
                const auto dst_path = paths.final() / path.filename();
                fs::create_directories(paths.final());
                fs::copy_file(
                    path, dst_path, fs::copy_options::overwrite_existing
                );
            }
        }

        return;
    }

}  // namespace dal
