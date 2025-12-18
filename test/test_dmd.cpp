#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "dal/auxiliary/byte_tool.hpp"
#include "dal/auxiliary/path.hpp"
#include "dal/parser/dmd/parser.h"
#include "dal/tools/dmd/exporter.h"
#include "dal/tools/scene/modifier.h"


#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define CHECK_TRUTH(condition)                                          \
    {                                                                   \
        if (!(condition))                                               \
            std::cout << "(" << (condition) << ") " TOSTRING(condition) \
                      << std::endl;                                     \
    }


namespace {

    constexpr unsigned NANOSEC_PER_SEC = 1000000000;

    double get_cur_sec(void) {
        return static_cast<double>(
                   std::chrono::steady_clock::now().time_since_epoch().count()
               ) /
               static_cast<double>(NANOSEC_PER_SEC);
    }

    std::vector<std::string> get_all_dir_within_folder(std::string folder) {
        std::vector<std::string> names;

        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            names.push_back(entry.path().filename().string());
        }

        return names;
    }

    std::filesystem::path find_root_path() {
        constexpr int DEPTH = 10;

        std::filesystem::path current_dir = ".";

        for (int i = 0; i < DEPTH; ++i) {
            for (const auto& entry :
                 std::filesystem::directory_iterator(current_dir)) {
                if (dal::tostr(entry.path().filename()) == ".git") {
                    return std::filesystem::absolute(current_dir);
                }
            }

            current_dir /= "..";
        }

        throw std::runtime_error{ "Failed to find root folder" };
    }


    std::vector<uint8_t> read_file(const char* const path) {
        std::ifstream file{ path,
                            std::ios::ate | std::ios::binary | std::ios::in };

        if (!file.is_open()) {
            throw std::runtime_error(
                std::string{ "failed to open file: " } + path
            );
        }

        const auto fileSize = static_cast<size_t>(file.tellg());
        std::vector<uint8_t> buffer;
        buffer.resize(fileSize);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        file.close();

        return buffer;
    }

    double compare_binary_buffers(
        const std::vector<uint8_t>& one, const std::vector<uint8_t>& two
    ) {
        if (one.size() != two.size()) {
            return 0.0;
        }

        size_t same_count = 0;

        for (size_t i = 0; i < one.size(); ++i) {
            if (one[i] == two[i]) {
                ++same_count;
            }
        }

        return static_cast<double>(same_count) /
               static_cast<double>(one.size());
    }

    void compare_models(const dal::Model& one, const dal::Model& two) {
        CHECK_TRUTH(one.aabb_.max_ == two.aabb_.max_);
        CHECK_TRUTH(one.aabb_.min_ == two.aabb_.min_);

        CHECK_TRUTH(
            one.skeleton_.joints_.size() == two.skeleton_.joints_.size()
        );
        for (size_t i = 0;
             i < std::min(
                     one.skeleton_.joints_.size(), two.skeleton_.joints_.size()
                 );
             ++i) {
            CHECK_TRUTH(
                one.skeleton_.joints_[i].name_ == two.skeleton_.joints_[i].name_
            );
            CHECK_TRUTH(
                one.skeleton_.joints_[i].joint_type_ ==
                two.skeleton_.joints_[i].joint_type_
            );
            CHECK_TRUTH(
                one.skeleton_.joints_[i].offset_mat_ ==
                two.skeleton_.joints_[i].offset_mat_
            );
            CHECK_TRUTH(
                one.skeleton_.joints_[i].parent_index_ ==
                two.skeleton_.joints_[i].parent_index_
            );
        }

        CHECK_TRUTH(one.animations_.size() == two.animations_.size());
        for (size_t i = 0;
             i < std::min(one.animations_.size(), two.animations_.size());
             ++i) {
            CHECK_TRUTH(one.animations_[i].name_ == two.animations_[i].name_);
            CHECK_TRUTH(
                one.animations_[i].calc_duration_in_ticks() ==
                two.animations_[i].calc_duration_in_ticks()
            );
            CHECK_TRUTH(
                one.animations_[i].ticks_per_sec_ ==
                two.animations_[i].ticks_per_sec_
            );

            CHECK_TRUTH(
                one.animations_[i].joints_.size() ==
                two.animations_[i].joints_.size()
            );
            for (size_t j = 0; j < std::min(
                                       one.animations_[i].joints_.size(),
                                       two.animations_[i].joints_.size()
                                   );
                 ++j) {
                CHECK_TRUTH(
                    one.animations_[i].joints_[j].name_ ==
                    two.animations_[i].joints_[j].name_
                );
                CHECK_TRUTH(
                    one.animations_[i].joints_[j].translations_ ==
                    two.animations_[i].joints_[j].translations_
                );
                CHECK_TRUTH(
                    one.animations_[i].joints_[j].rotations_ ==
                    two.animations_[i].joints_[j].rotations_
                );
                CHECK_TRUTH(
                    one.animations_[i].joints_[j].scales_ ==
                    two.animations_[i].joints_[j].scales_
                );
            }
        }

        CHECK_TRUTH(one.units_straight_.size() == two.units_straight_.size());
        for (size_t i = 0;
             i <
             std::min(one.units_straight_.size(), two.units_straight_.size());
             ++i) {
            CHECK_TRUTH(
                one.units_straight_[i].name_ == two.units_straight_[i].name_
            );
            CHECK_TRUTH(
                one.units_straight_[i].material_ ==
                two.units_straight_[i].material_
            );
            CHECK_TRUTH(
                one.units_straight_[i].mesh_.vertices_ ==
                two.units_straight_[i].mesh_.vertices_
            );
            CHECK_TRUTH(
                one.units_straight_[i].mesh_.uv_coordinates_ ==
                two.units_straight_[i].mesh_.uv_coordinates_
            );
            CHECK_TRUTH(
                one.units_straight_[i].mesh_.normals_ ==
                two.units_straight_[i].mesh_.normals_
            );
        }

        CHECK_TRUTH(
            one.units_straight_joint_.size() == two.units_straight_joint_.size()
        );
        for (size_t i = 0; i < std::min(
                                   one.units_straight_joint_.size(),
                                   two.units_straight_joint_.size()
                               );
             ++i) {
            CHECK_TRUTH(
                one.units_straight_joint_[i].name_ ==
                two.units_straight_joint_[i].name_
            );
            CHECK_TRUTH(
                one.units_straight_joint_[i].material_ ==
                two.units_straight_joint_[i].material_
            );
            CHECK_TRUTH(
                one.units_straight_joint_[i].mesh_.vertices_ ==
                two.units_straight_joint_[i].mesh_.vertices_
            );
            CHECK_TRUTH(
                one.units_straight_joint_[i].mesh_.uv_coordinates_ ==
                two.units_straight_joint_[i].mesh_.uv_coordinates_
            );
            CHECK_TRUTH(
                one.units_straight_joint_[i].mesh_.normals_ ==
                two.units_straight_joint_[i].mesh_.normals_
            );
        }

        CHECK_TRUTH(one.units_indexed_.size() == two.units_indexed_.size());
        CHECK_TRUTH(
            one.units_indexed_joint_.size() == two.units_indexed_joint_.size()
        );
    }

}  // namespace


namespace {

    void test_a_model(const char* const model_path) {
        std::cout << "< " << model_path << " >" << std::endl;

        const auto file_content = ::read_file(model_path);
        const auto model = dal::parse_dmd(file_content);

        std::cout << "    * Loaded and parsed" << std::endl;
        std::cout << "        render units straight:       "
                  << model->units_straight_.size() << std::endl;
        std::cout << "        render units straight joint: "
                  << model->units_straight_joint_.size() << std::endl;
        std::cout << "        render units indexed:        "
                  << model->units_indexed_.size() << std::endl;
        std::cout << "        render units indexed joint:  "
                  << model->units_indexed_joint_.size() << std::endl;
        std::cout << "        joints: " << model->skeleton_.joints_.size()
                  << std::endl;
        std::cout << "        animations: " << model->animations_.size()
                  << std::endl;

        {
            const auto binary = dal::build_binary_model(
                *model, dal::CompressMethod::brotli
            );
            const auto model_second = dal::parse_dmd(*binary);

            std::cout << "    * Second model parsed" << std::endl;
            std::cout << "        render units straight:       "
                      << model_second->units_straight_.size() << std::endl;
            std::cout << "        render units straight joint: "
                      << model_second->units_straight_joint_.size()
                      << std::endl;
            std::cout << "        render units indexed:        "
                      << model_second->units_indexed_.size() << std::endl;
            std::cout << "        render units indexed joint:  "
                      << model_second->units_indexed_joint_.size() << std::endl;
            std::cout << "        joints: "
                      << model_second->skeleton_.joints_.size() << std::endl;
            std::cout << "        animations: "
                      << model_second->animations_.size() << std::endl;

            std::cout << "    * Built binary" << std::endl;
            std::cout << "        original binary size: " << file_content.size()
                      << std::endl;
            std::cout << "        built    binary size: " << binary->size()
                      << std::endl;
            std::cout << "        compare: "
                      << ::compare_binary_buffers(file_content, *binary)
                      << std::endl;

            ::compare_models(*model, *model_second);
        }

        {
            const auto merged_0 = dal::merge_by_material(
                model->units_straight_
            );
            const auto merged_1 = dal::merge_by_material(
                model->units_straight_joint_
            );
            const auto merged_2 = dal::merge_by_material(model->units_indexed_);
            const auto merged_3 = dal::merge_by_material(
                model->units_indexed_joint_
            );

            const auto before = model->units_straight_.size() +
                                model->units_straight_joint_.size() +
                                model->units_indexed_.size() +
                                model->units_indexed_joint_.size();
            const auto after = merged_0.size() + merged_1.size() +
                               merged_2.size() + merged_3.size();

            std::cout << "    * Merging by material" << std::endl;
            std::cout << "        before: " << before << std::endl;
            std::cout << "        after : " << after << std::endl;
        }

        {
            std::cout << "    * Reducing joints" << std::endl;
            auto model_cpy = model.value();
            std::cout << "        result: "
                      << (dal::JointReductionResult::fail !=
                          dal::reduce_joints(model_cpy))
                      << std::endl;
        }

        constexpr double TEST_DURATION = 0.5;

        {
            std::cout << "    * Measuring import performance" << std::endl;

            const auto start_time = ::get_cur_sec();
            size_t process_count = 0;

            while (::get_cur_sec() - start_time < TEST_DURATION) {
                const auto model = dal::parse_dmd(file_content);
                ++process_count;
            }

            std::cout << "        processed "
                      << static_cast<double>(process_count) / TEST_DURATION
                      << " times per sec\n";
        }

        {
            std::cout << "    * Measuring export performance" << std::endl;

            const auto start_time = ::get_cur_sec();
            size_t process_count = 0;

            while (::get_cur_sec() - start_time < TEST_DURATION) {
                const auto binary = dal::build_binary_model(
                    *model, dal::CompressMethod::brotli
                );
                ++process_count;
            }

            std::cout << "        processed "
                      << static_cast<double>(process_count) / TEST_DURATION
                      << " times per sec\n";
        }
    }

    void test_a_model(const std::string& model_path) {
        ::test_a_model(model_path.c_str());
    }

    void create_indexed_model(
        const char* const dst_path, const char* const src_path
    ) {
        std::cout << "Convert " << src_path << " to indexed model to "
                  << dst_path;

        const auto model_data = ::read_file(dst_path);
        auto model = dal::parse_dmd(model_data);

        for (const auto& unit : model->units_straight_) {
            dal::RenderUnit<dal::Mesh_Indexed> new_unit;
            new_unit.name_ = unit.name_;
            new_unit.material_ = unit.material_;
            new_unit.mesh_ = dal::convert_to_indexed(unit.mesh_);
            model->units_indexed_.push_back(new_unit);
        }
        model->units_straight_.clear();

        for (const auto& unit : model->units_straight_joint_) {
            dal::RenderUnit<dal::Mesh_IndexedJoint> new_unit;
            new_unit.name_ = unit.name_;
            new_unit.material_ = unit.material_;
            new_unit.mesh_ = dal::convert_to_indexed(unit.mesh_);
            model->units_indexed_joint_.push_back(new_unit);
        }
        model->units_straight_joint_.clear();

        const auto binary_built = dal::build_binary_model(
            *model, dal::CompressMethod::brotli
        );

        std::ofstream file(src_path, std::ios::binary);
        file.write(
            reinterpret_cast<const char*>(binary_built->data()),
            binary_built->size()
        );
        file.close();

        std::cout << " -> Done" << std::endl;
    }

    void create_indexed_model(
        const std::string& dst_path, const std::string& src_path
    ) {
        ::create_indexed_model(dst_path.c_str(), src_path.c_str());
    }

}  // namespace


int main() {
    for (auto entry : std::filesystem::directory_iterator(
             ::find_root_path() / "test" / "dmd"
         )) {
        if (entry.path().extension().string() == ".dmd") {
            std::cout << std::endl;
            ::test_a_model(entry.path().string());
        }
    }
}
