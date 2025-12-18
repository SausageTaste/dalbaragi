#include "dal/parser/dmd/parser.hpp"

#include <stdexcept>

#include <sung/basic/byte_arr.hpp>

#include "dal/auxiliary/byte_tool.hpp"
#include "dal/parser/common/decompress.hpp"
#include "dal/parser/common/konst.hpp"


namespace {

    std::optional<std::vector<uint8_t>> unzip_dal_model(sung::BytesReader& r) {
        const auto comp_method = (dal::CompressMethod)r.read_int32().value();
        const auto expected_unzipped_size = r.read_int64().value();

        if (comp_method == dal::CompressMethod::none) {
            std::vector<uint8_t> output{ r.head(), r.head() + r.remaining() };
            return output;
        } else if (comp_method == dal::CompressMethod::zip) {
            const auto unzipped = dal::decomp_zip(
                dal::BinDataView{ r.head(), r.remaining() },
                expected_unzipped_size
            );
            if (unzipped.has_value())
                return unzipped.value();
            else
                return std::nullopt;
        } else if (comp_method == dal::CompressMethod::brotli) {
            const auto unzipped = dal::decomp_bro(
                r.head(), r.remaining(), expected_unzipped_size
            );

            if (unzipped.has_value())
                return unzipped.value();
            else
                return std::nullopt;
        }


        const auto unzipped = dal::decomp_bro(
            r.head(), r.remaining(), expected_unzipped_size
        );

        if (unzipped.has_value())
            return unzipped.value();
        else
            return std::nullopt;
    }

    bool is_magic_numbers_correct(const uint8_t* const buf) {
        for (int i = 0; i < dal::MAGIC_NUMBER_SIZE; ++i) {
            if (buf[i] != dal::MAGIC_NUMBERS_DAL_MODEL[i]) {
                return false;
            }
        }

        return true;
    }

}  // namespace


// Parser functions
namespace {

    void parse_aabb(sung::BytesReader& r, dal::AABB3& output) {
        output.min_.x = r.read_float32().value();
        output.min_.y = r.read_float32().value();
        output.min_.z = r.read_float32().value();
        output.max_.x = r.read_float32().value();
        output.max_.y = r.read_float32().value();
        output.max_.z = r.read_float32().value();
    }

    void parse_mat4(sung::BytesReader& r, glm::mat4& mat) {
        for (glm::mat4::length_type row = 0; row < 4; ++row) {
            for (glm::mat4::length_type col = 0; col < 4; ++col) {
                mat[col][row] = r.read_float32().value();
            }
        }
    }

}  // namespace


// Parse animations
namespace {

    void parse_skeleton(sung::BytesReader& r, dal::Skeleton& output) {
        ::parse_mat4(r, output.root_transform_);

        const auto joint_count = r.read_int32().value();
        output.joints_.resize(joint_count);

        for (int i = 0; i < joint_count; ++i) {
            auto& joint = output.joints_.at(i);

            joint.name_ = r.read_nt_str();
            joint.parent_index_ = r.read_int32().value();

            const auto joint_type_index = r.read_int32().value();
            switch (joint_type_index) {
                case 0:
                    joint.joint_type_ = dal::JointType::basic;
                    break;
                case 1:
                    joint.joint_type_ = dal::JointType::hair_root;
                    break;
                case 2:
                    joint.joint_type_ = dal::JointType::skirt_root;
                    break;
                default:
                    joint.joint_type_ = dal::JointType::basic;
            }

            ::parse_mat4(r, joint.offset_mat_);
        }
    }

    void parse_animJoint(sung::BytesReader& r, dal::AnimJoint& output) {
        {
            output.name_ = r.read_nt_str();

            glm::mat4 _;
            ::parse_mat4(r, _);
        }

        {
            const auto num = r.read_int32().value();
            for (int i = 0; i < num; ++i) {
                // The order of parameter evaluation is not coherent with the
                // order of the parameters so I must use a buffer
                const auto fbuf = r.read_float32_arr<4>().value();
                output.add_position(fbuf[0], fbuf[1], fbuf[2], fbuf[3]);
            }
        }

        {
            const auto num = r.read_int32().value();
            for (int i = 0; i < num; ++i) {
                const auto fbuf = r.read_float32_arr<5>().value();
                output.add_rotation(
                    fbuf[0], fbuf[1], fbuf[2], fbuf[3], fbuf[4]
                );
            }
        }

        {
            const auto num = r.read_int32().value();
            for (int i = 0; i < num; ++i) {
                const auto fbuf = r.read_float32_arr<2>().value();
                output.add_scale(fbuf[0], fbuf[1]);
            }
        }
    }

    void parse_animations(
        sung::BytesReader& r, std::vector<dal::Animation>& animations
    ) {
        const auto anim_count = r.read_int32().value();
        animations.resize(anim_count);
        for (int i = 0; i < anim_count; ++i) {
            auto& anim = animations.at(i);

            anim.name_ = r.read_nt_str();
            const auto duration_tick = r.read_float32().value();
            anim.ticks_per_sec_ = r.read_float32().value();

            const auto joint_count = r.read_int32().value();
            anim.joints_.resize(joint_count);
            for (int j = 0; j < joint_count; ++j) {
                ::parse_animJoint(r, anim.joints_.at(j));
            }
        }
    }

}  // namespace


// Parse render units
namespace {

    void parse_material(sung::BytesReader& r, dal::Material& material) {
        material.roughness_ = r.read_float32().value();
        material.metallic_ = r.read_float32().value();
        material.transparency_ = r.read_bool().value();

        material.albedo_map_ = r.read_nt_str();
        material.roughness_map_ = r.read_nt_str();
        material.metallic_map_ = r.read_nt_str();
        material.normal_map_ = r.read_nt_str();
    }

    void parse_mesh(sung::BytesReader& r, dal::Mesh_Straight& mesh) {
        const auto vert_count = r.read_int64().value();
        const auto vert_count_x_3 = vert_count * 3;
        const auto vert_count_x_2 = vert_count * 2;

        mesh.vertices_.resize(vert_count_x_3);
        if (!r.read_float32_arr(mesh.vertices_.data(), vert_count_x_3))
            throw std::runtime_error{ "Failed to read vertices" };

        mesh.uv_coordinates_.resize(vert_count_x_2);
        if (!r.read_float32_arr(mesh.uv_coordinates_.data(), vert_count_x_2))
            throw std::runtime_error{ "Failed to read uv coordinates" };

        mesh.normals_.resize(vert_count_x_3);
        if (!r.read_float32_arr(mesh.normals_.data(), vert_count_x_3))
            throw std::runtime_error{ "Failed to read normals" };
    }

    void parse_mesh(sung::BytesReader& r, dal::Mesh_StraightJoint& mesh) {
        const auto vert_count = r.read_int64().value();
        const auto vert_count_x_3 = vert_count * 3;
        const auto vert_count_x_2 = vert_count * 2;
        const auto vert_count_joint = vert_count * dal::NUM_JOINTS_PER_VERTEX;

        mesh.vertices_.resize(vert_count_x_3);
        if (!r.read_float32_arr(mesh.vertices_.data(), vert_count_x_3))
            throw std::runtime_error{ "Failed to read vertices" };

        mesh.uv_coordinates_.resize(vert_count_x_2);
        if (!r.read_float32_arr(mesh.uv_coordinates_.data(), vert_count_x_2))
            throw std::runtime_error{ "Failed to read uv coordinates" };

        mesh.normals_.resize(vert_count_x_3);
        if (!r.read_float32_arr(mesh.normals_.data(), vert_count_x_3))
            throw std::runtime_error{ "Failed to read normals" };

        mesh.joint_weights_.resize(vert_count_joint);
        if (!r.read_float32_arr(mesh.joint_weights_.data(), vert_count_joint))
            throw std::runtime_error{ "Failed to read joint weights" };

        mesh.joint_indices_.resize(vert_count_joint);
        if (!r.read_int32_arr(mesh.joint_indices_.data(), vert_count_joint))
            throw std::runtime_error{ "Failed to read joint indices" };
    }

    void parse_mesh(sung::BytesReader& r, dal::Mesh_Indexed& mesh) {
        const auto vertex_count = r.read_int64().value();
        for (int64_t i = 0; i < vertex_count; ++i) {
            auto& vert = mesh.vertices_.emplace_back();

            static_assert(sizeof(float) * 3 == sizeof(vert.pos_), "");
            static_assert(sizeof(float) * 3 == sizeof(vert.normal_), "");
            static_assert(sizeof(float) * 2 == sizeof(vert.uv_), "");

            if (!r.read_float32_arr(&vert.pos_[0], 3))
                throw std::runtime_error{ "Failed to read pos" };
            if (!r.read_float32_arr(&vert.normal_[0], 3))
                throw std::runtime_error{ "Failed to read normal" };
            if (!r.read_float32_arr(&vert.uv_[0], 2))
                throw std::runtime_error{ "Failed to read uv" };
        }

        const auto index_count = r.read_int64().value();
        for (int64_t i = 0; i < index_count; ++i)
            mesh.indices_.push_back(r.read_int32().value());
    }

    void parse_mesh(sung::BytesReader& r, dal::Mesh_IndexedJoint& mesh) {
        constexpr auto J_ELEM = dal::NUM_JOINTS_PER_VERTEX;
        const auto vertex_count = r.read_int64().value();

        for (int64_t i = 0; i < vertex_count; ++i) {
            auto& vert = mesh.vertices_.emplace_back();

            static_assert(
                sizeof(vert.joint_weights_) ==
                sizeof(float) * dal::NUM_JOINTS_PER_VERTEX
            );
            static_assert(
                sizeof(vert.joint_indices_) ==
                sizeof(int32_t) * dal::NUM_JOINTS_PER_VERTEX
            );

            if (!r.read_float32_arr(&vert.pos_[0], 3))
                throw std::runtime_error{ "Failed to read pos" };
            if (!r.read_float32_arr(&vert.normal_[0], 3))
                throw std::runtime_error{ "Failed to read normal" };
            if (!r.read_float32_arr(&vert.uv_[0], 2))
                throw std::runtime_error{ "Failed to read uv" };
            if (!r.read_float32_arr(&vert.joint_weights_[0], J_ELEM))
                throw std::runtime_error{ "Failed to read joint weights" };
            if (!r.read_int32_arr(&vert.joint_indices_[0], J_ELEM))
                throw std::runtime_error{ "Failed to read joint indices" };
        }

        const auto index_count = r.read_int64().value();
        for (int64_t i = 0; i < index_count; ++i)
            mesh.indices_.push_back(r.read_int32().value());
    }

    template <typename _Mesh>
    void parse_render_unit(sung::BytesReader& r, dal::RenderUnit<_Mesh>& unit) {
        unit.name_ = r.read_nt_str();
        ::parse_material(r, unit.material_);
        ::parse_mesh(r, unit.mesh_);
    }


    dal::ModelParseResult parse_all(sung::BytesReader& r, dal::Model& output) {
        ::parse_aabb(r, output.aabb_);
        ::parse_skeleton(r, output.skeleton_);
        ::parse_animations(r, output.animations_);

        output.units_straight_.resize(r.read_int64().value());
        for (auto& unit : output.units_straight_) ::parse_render_unit(r, unit);

        output.units_straight_joint_.resize(r.read_int64().value());
        for (auto& unit : output.units_straight_joint_)
            ::parse_render_unit(r, unit);

        output.units_indexed_.resize(r.read_int64().value());
        for (auto& unit : output.units_indexed_) ::parse_render_unit(r, unit);

        output.units_indexed_joint_.resize(r.read_int64().value());
        for (auto& unit : output.units_indexed_joint_)
            ::parse_render_unit(r, unit);

        if (r.is_eof())
            return dal::ModelParseResult::success;
        else
            return dal::ModelParseResult::corrupted_content;
    }

}  // namespace


namespace dal {

    ModelParseResult parse_dmd(
        Model& output,
        const uint8_t* const file_content,
        const size_t content_size
    ) {
        // Check magic numbers
        if (!::is_magic_numbers_correct(file_content))
            return dal::ModelParseResult::magic_numbers_dont_match;

        sung::BytesReader file_bytes{ file_content, content_size };
        file_bytes.advance(dal::MAGIC_NUMBER_SIZE);

        const auto comp_method_i = file_bytes.read_int32().value();
        const auto expected_unzipped_size = file_bytes.read_int64().value();
        const auto comp_method = (CompressMethod)comp_method_i;

        if (comp_method == CompressMethod::none) {
            return ::parse_all(file_bytes, output);
        } else {
            std::optional<std::vector<uint8_t>> unzipped = std::nullopt;

            if (comp_method == CompressMethod::zip) {
                unzipped = decomp_zip(
                    BinDataView{ file_bytes.head(), file_bytes.remaining() },
                    expected_unzipped_size
                );
            } else if (comp_method == CompressMethod::brotli) {
                unzipped = decomp_bro(
                    file_bytes.head(),
                    file_bytes.remaining(),
                    expected_unzipped_size
                );
            }

            if (!unzipped.has_value())
                return dal::ModelParseResult::decompression_failed;

            sung::BytesReader r{ unzipped->data(), unzipped->size() };
            return ::parse_all(r, output);
        }

        return dal::ModelParseResult::corrupted_content;
    }

    std::optional<Model> parse_dmd(
        const uint8_t* const file_content, const size_t content_size
    ) {
        Model output;

        if (ModelParseResult::success !=
            dal::parse_dmd(output, file_content, content_size))
            return std::nullopt;
        else
            return output;
    }

    std::optional<Model> parse_dmd(const BinDataView& src) {
        return dal::parse_dmd(src.data(), src.size());
    }

}  // namespace dal
