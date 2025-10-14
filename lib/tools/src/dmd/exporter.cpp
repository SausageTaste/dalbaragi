#include "daltools/dmd/exporter.h"

#include "daltools/common/byte_tool.h"
#include "daltools/common/compression.h"
#include "daltools/common/konst.h"


namespace dalp = dal::parser;


namespace {

    class BinaryBuildBuffer : public dalp::BinaryDataArray {

    public:
        void append_float32_vector(const std::vector<float>& v) {
            this->append_float32_array(v.data(), v.size());
        }

        void append_mat4(const glm::mat4& mat) {
            float fbuf[16];

            for (uint32_t row = 0; row < 4; ++row) {
                for (uint32_t col = 0; col < 4; ++col) {
                    fbuf[4 * row + col] = mat[col][row];
                }
            }

            this->append_float32_array(fbuf, 16);
        }

        void append_raw_array(const uint8_t* const arr, const size_t arr_size) {
            this->append_array(arr, arr_size);
        }
    };

}  // namespace


namespace {

    std::optional<std::vector<uint8_t>> compress_dal_model(
        const uint8_t* const src,
        const size_t src_size,
        dal::CompressMethod comp_method
    ) {
        dalp::BinaryDataArray output;

        output.append_array(
            dalp::MAGIC_NUMBERS_DAL_MODEL, dalp::MAGIC_NUMBER_SIZE
        );
        output.append_int32(static_cast<int32_t>(comp_method));
        output.append_int64(src_size);

        if (comp_method == dal::CompressMethod::none) {
            output.append_array(src, src_size);
        } else if (comp_method == dal::CompressMethod::zip) {
            dal::BinDataView bin_view{ src, src_size };
            const auto compressed = dal::compress_zip(bin_view);
            if (!compressed.has_value())
                return std::nullopt;
            output.append_array(compressed->data(), compressed->size());
        } else if (comp_method == dal::CompressMethod::brotli) {
            const auto compressed = dal::compress_bro(src, src_size);
            if (!compressed.has_value())
                return std::nullopt;
            output.append_array(compressed->data(), compressed->size());
        } else {
            return std::nullopt;
        }

        return output.release();
    }

    void append_bin_aabb(::BinaryBuildBuffer& output, const dalp::AABB3& aabb) {
        output.append_float32(aabb.min_.x);
        output.append_float32(aabb.min_.y);
        output.append_float32(aabb.min_.z);
        output.append_float32(aabb.max_.x);
        output.append_float32(aabb.max_.y);
        output.append_float32(aabb.max_.z);
    }

}  // namespace


// Build animations
namespace {

    void build_bin_skeleton(
        ::BinaryBuildBuffer& output, const dalp::Skeleton& skeleton
    ) {
        output.append_mat4(skeleton.root_transform_);
        output.append_int32(skeleton.joints_.size());

        for (size_t i = 0; i < skeleton.joints_.size(); ++i) {
            const auto& joint = skeleton.joints_[i];

            output.append_str(joint.name_);
            output.append_int32(joint.parent_index_);

            switch (joint.joint_type_) {
                case dalp::JointType::basic:
                    output.append_int32(0);
                    break;
                case dalp::JointType::hair_root:
                    output.append_int32(1);
                    break;
                case dalp::JointType::skirt_root:
                    output.append_int32(2);
                    break;
                default:
                    assert(false);
            }

            output.append_mat4(joint.offset_mat_);
        }
    }

    void _build_bin_joint_keyframes(
        ::BinaryBuildBuffer& output, const dalp::AnimJoint& joint
    ) {
        output.append_str(joint.name_);
        output.append_mat4(glm::mat4{ 1 });

        output.append_int32(joint.translations_.size());
        for (auto& trans : joint.translations_) {
            output.append_float32(trans.first);
            output.append_float32(trans.second.x);
            output.append_float32(trans.second.y);
            output.append_float32(trans.second.z);
        }

        output.append_int32(joint.rotations_.size());
        for (auto& rot : joint.rotations_) {
            output.append_float32(rot.first);
            output.append_float32(rot.second.w);
            output.append_float32(rot.second.x);
            output.append_float32(rot.second.y);
            output.append_float32(rot.second.z);
        }

        output.append_int32(joint.scales_.size());
        for (auto& scale : joint.scales_) {
            output.append_float32(scale.first);
            output.append_float32(scale.second);
        }
    }

    void build_bin_animation(
        ::BinaryBuildBuffer& output,
        const std::vector<dalp::Animation>& animations
    ) {
        output.append_int32(animations.size());

        for (size_t i = 0; i < animations.size(); ++i) {
            auto& anim = animations[i];

            output.append_str(anim.name_);
            output.append_float32(anim.calc_duration_in_ticks());
            output.append_float32(anim.ticks_per_sec_);

            output.append_int32(anim.joints_.size());

            for (auto& joint : anim.joints_) {
                ::_build_bin_joint_keyframes(output, joint);
            }
        }
    }

}  // namespace


// Build render units
namespace {

    void build_bin_material(
        ::BinaryBuildBuffer& output, const dalp::Material& material
    ) {
        output.append_float32(material.roughness_);
        output.append_float32(material.metallic_);
        output.append_bool8(material.transparency_);
        output.append_str(material.albedo_map_);
        output.append_str(material.roughness_map_);
        output.append_str(material.metallic_map_);
        output.append_str(material.normal_map_);
    }

    void build_bin_mesh_straight(
        ::BinaryBuildBuffer& output, const dalp::Mesh_Straight& mesh
    ) {
        assert(mesh.vertices_.size() * 2 == mesh.uv_coordinates_.size() * 3);
        assert(mesh.vertices_.size() == mesh.normals_.size());
        assert(mesh.vertices_.size() % 3 == 0);
        const auto nuvertices_ = mesh.vertices_.size() / 3;

        output.append_int64(nuvertices_);
        output.append_float32_vector(mesh.vertices_);
        output.append_float32_vector(mesh.uv_coordinates_);
        output.append_float32_vector(mesh.normals_);
    }

    void build_bin_mesh_straight_joint(
        ::BinaryBuildBuffer& output, const dalp::Mesh_StraightJoint& mesh
    ) {
        assert(
            mesh.vertices_.size() * dal::parser::NUM_JOINTS_PER_VERTEX ==
            mesh.joint_indices_.size() * 3
        );
        assert(
            mesh.vertices_.size() * dal::parser::NUM_JOINTS_PER_VERTEX ==
            mesh.joint_weights_.size() * 3
        );

        ::build_bin_mesh_straight(output, mesh);

        output.append_float32_vector(mesh.joint_weights_);
        output.append_int32_array(
            mesh.joint_indices_.data(), mesh.joint_indices_.size()
        );
    }

    void build_bin_mesh_indexed(
        ::BinaryBuildBuffer& output, const dalp::Mesh_Indexed& mesh
    ) {
        static_assert(32 == sizeof(dalp::Mesh_Indexed::VERT_TYPE));

        output.append_int64(mesh.vertices_.size());
        for (auto& vert : mesh.vertices_) {
            output.append_float32(vert.pos_.x);
            output.append_float32(vert.pos_.y);
            output.append_float32(vert.pos_.z);

            output.append_float32(vert.normal_.x);
            output.append_float32(vert.normal_.y);
            output.append_float32(vert.normal_.z);

            output.append_float32(vert.uv_.x);
            output.append_float32(vert.uv_.y);
        }

        output.append_int64(mesh.indices_.size());
        for (auto index : mesh.indices_) {
            output.append_int32(index);
        }
    }

    void build_bin_mesh_indexed_joint(
        ::BinaryBuildBuffer& output, const dalp::Mesh_IndexedJoint& mesh
    ) {
        static_assert(64 == sizeof(dalp::Mesh_IndexedJoint::VERT_TYPE));

        output.append_int64(mesh.vertices_.size());
        for (auto& vert : mesh.vertices_) {
            output.append_float32(vert.pos_.x);
            output.append_float32(vert.pos_.y);
            output.append_float32(vert.pos_.z);

            output.append_float32(vert.normal_.x);
            output.append_float32(vert.normal_.y);
            output.append_float32(vert.normal_.z);

            output.append_float32(vert.uv_.x);
            output.append_float32(vert.uv_.y);

            output.append_float32(vert.joint_weights_.x);
            output.append_float32(vert.joint_weights_.y);
            output.append_float32(vert.joint_weights_.z);
            output.append_float32(vert.joint_weights_.w);

            output.append_int32(vert.joint_indices_.x);
            output.append_int32(vert.joint_indices_.y);
            output.append_int32(vert.joint_indices_.z);
            output.append_int32(vert.joint_indices_.w);
        }

        output.append_int64(mesh.indices_.size());
        for (auto index : mesh.indices_) {
            output.append_int32(index);
        }
    }

}  // namespace


namespace dal::parser {

    ModelExportResult build_binary_model(
        std::vector<uint8_t>& output,
        const Model& input,
        CompressMethod comp_method
    ) {
        BinaryBuildBuffer buffer;

        ::append_bin_aabb(buffer, input.aabb_);
        ::build_bin_skeleton(buffer, input.skeleton_);
        ::build_bin_animation(buffer, input.animations_);

        buffer.append_int64(input.units_straight_.size());
        for (auto& unit : input.units_straight_) {
            buffer.append_str(unit.name_);
            ::build_bin_material(buffer, unit.material_);
            ::build_bin_mesh_straight(buffer, unit.mesh_);
        }

        buffer.append_int64(input.units_straight_joint_.size());
        for (auto& unit : input.units_straight_joint_) {
            buffer.append_str(unit.name_);
            ::build_bin_material(buffer, unit.material_);
            ::build_bin_mesh_straight_joint(buffer, unit.mesh_);
        }

        buffer.append_int64(input.units_indexed_.size());
        for (auto& unit : input.units_indexed_) {
            buffer.append_str(unit.name_);
            ::build_bin_material(buffer, unit.material_);
            ::build_bin_mesh_indexed(buffer, unit.mesh_);
        }

        buffer.append_int64(input.units_indexed_joint_.size());
        for (auto& unit : input.units_indexed_joint_) {
            buffer.append_str(unit.name_);
            ::build_bin_material(buffer, unit.material_);
            ::build_bin_mesh_indexed_joint(buffer, unit.mesh_);
        }

        auto zipped = ::compress_dal_model(
            buffer.data(), buffer.size(), comp_method
        );
        if (!zipped.has_value())
            return ModelExportResult::compression_failure;

        output = std::move(zipped.value());
        return ModelExportResult::success;
    }

    std::optional<std::vector<uint8_t>> build_binary_model(
        const Model& input, CompressMethod comp_method
    ) {
        std::vector<uint8_t> result;
        const auto res = build_binary_model(result, input, comp_method);
        if (ModelExportResult::success != res)
            return std::nullopt;
        else
            return result;
    }

}  // namespace dal::parser
