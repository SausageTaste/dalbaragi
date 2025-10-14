#include "daltools/scene/modifier.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include <sung/basic/aabb.hpp>
#include <sung/basic/geometry2d.hpp>

#include "daltools/common/glm_tool.hpp"
#include "daltools/img/backend/ktx.hpp"
#include "daltools/img/img.hpp"
#include "daltools/img/img2d.hpp"


namespace {

    namespace fs = std::filesystem;
    using scene_t = dal::SceneIntermediate;


    template <typename T>
    class EasyMap {

    private:
        std::unordered_map<T, T> m_map;

    public:
        void add(const T& key, const T& value) {
            if (this->m_map.end() != this->m_map.find(key)) {
                throw std::runtime_error{
                    "Trying to add a 'key' which already exists"
                };
            }

            this->m_map[key] = value;
        }

        auto& get(const std::string& key) const {
            const auto found = this->m_map.find(key);
            if (this->m_map.end() != found) {
                return found->second;
            } else {
                return key;
            }
        }
    };


    class StringReplaceMap {

    private:
        ::EasyMap<std::string> m_map;

    public:
        void add(const std::string& froname_, const std::string& to_name) {
            if (froname_ == to_name) {
                throw std::runtime_error{
                    "froname_ and to_name are identical, maybe ill-formed data"
                };
            }

            this->m_map.add(froname_, to_name);
        }

        auto& get(const std::string& froname_) const {
            return this->m_map.get(froname_);
        }
    };

}  // namespace


// apply_root_transform
namespace {

    void apply_transform(const glm::mat4& m, glm::vec3& v) {
        v = m * glm::vec4{ v, 1 };
    }

    void apply_transform(const glm::mat3& m, glm::quat& q) {
        static_assert(0 * sizeof(float) == offsetof(glm::quat, x));
        static_assert(1 * sizeof(float) == offsetof(glm::quat, y));
        static_assert(2 * sizeof(float) == offsetof(glm::quat, z));
        static_assert(3 * sizeof(float) == offsetof(glm::quat, w));
        static_assert(3 * sizeof(float) == sizeof(glm::vec3));

        auto& quat_rotation_part = *reinterpret_cast<glm::vec3*>(&q.x);
        quat_rotation_part = m * quat_rotation_part;
    }

    template <typename T>
    T combine_abs_value_and_sign(const T only_abs_value, const T only_sign) {
        const auto ZERO = static_cast<T>(0);

        if (only_abs_value < ZERO) {
            if (only_sign < ZERO) {
                return only_abs_value;
            } else {
                return -only_abs_value;
            }
        } else {
            if (only_sign < ZERO) {
                return -only_abs_value;
            } else {
                return only_abs_value;
            }
        }
    }

    void rotate_scale_factors(const glm::mat3& m, glm::vec3& scales) {
        const auto rotated = m * scales;
        scales = glm::vec3{
            ::combine_abs_value_and_sign(rotated[0], scales[0]),
            ::combine_abs_value_and_sign(rotated[1], scales[1]),
            ::combine_abs_value_and_sign(rotated[2], scales[2])
        };
    }

    void apply_transform(
        const glm::mat4& m4, const glm::mat3& m3, scene_t::Transform& t
    ) {
        ::apply_transform(m4, t.pos_);
        ::apply_transform(m3, t.quat_);
        ::rotate_scale_factors(m3, t.scale_);
    }

}  // namespace


// convert_to_model_dmd
namespace {

    template <typename T>
    dal::RenderUnit<T>& find_or_create_render_unit_by_material(
        std::vector<dal::RenderUnit<T>>& units,
        const scene_t::Mesh& src_mesh,
        const scene_t::Material& src_mat
    ) {
        constexpr auto MAX_IDX = std::numeric_limits<uint32_t>::max();

        for (auto& unit : units) {
            if (unit.material_.is_physically_same(src_mat)) {
                const auto unit_size = unit.mesh_.vertices_.size();
                const auto src_size = src_mesh.indices_.size();
                const auto worst_size = unit_size + src_size;

                if (worst_size < MAX_IDX)
                    return unit;
            }
        }

        return units.emplace_back();
    }

    void convert_meshes(
        dal::Model& output, const dal::SceneIntermediate& scene
    ) {
        for (auto& src_mesh_actor : scene.mesh_actors_) {
            const auto actor_mat4 = scene.make_hierarchy_transform(
                src_mesh_actor
            );
            const auto actor_mat3 = glm::mat3{ actor_mat4 };

            for (auto& pair : src_mesh_actor.render_pairs_) {
                const auto src_mesh = scene.find_mesh_by_name(pair.mesh_name_);
                if (nullptr == src_mesh) {
                    throw std::runtime_error{ "" };
                }
                const auto src_material = scene.find_material_by_name(
                    pair.material_name_
                );
                if (nullptr == src_material) {
                    std::cout
                        << "Failed to find a material: " << pair.material_name_
                        << '\n';
                    continue;
                }

                if (src_mesh->skeleton_name_.empty()) {
                    auto& dst_pair = ::find_or_create_render_unit_by_material(
                        output.units_indexed_, *src_mesh, *src_material
                    );

                    dst_pair.name_ = src_mesh->name_;
                    dst_pair.material_ = *src_material;

                    for (auto src_index : src_mesh->indices_) {
                        auto& src_vert = src_mesh->vertices_[src_index];

                        dal::Vertex vertex;
                        vertex.pos_ = actor_mat4 *
                                      glm::vec4{ src_vert.pos_, 1 };
                        vertex.uv_ = src_vert.uv_;
                        vertex.normal_ = glm::normalize(
                            actor_mat3 * src_vert.normal_
                        );
                        dst_pair.mesh_.add_vertex(vertex);
                    }
                } else {
                    auto& dst_pair = ::find_or_create_render_unit_by_material(
                        output.units_indexed_joint_, *src_mesh, *src_material
                    );

                    dst_pair.name_ = src_mesh->name_;
                    dst_pair.material_ = *src_material;

                    for (auto src_index : src_mesh->indices_) {
                        auto& src_vert = src_mesh->vertices_[src_index];

                        dal::VertexJoint vertex;
                        vertex.joint_indices_ = src_vert.j_indices_;
                        vertex.joint_weights_ = src_vert.j_weights_;
                        vertex.pos_ = actor_mat4 *
                                      glm::vec4{ src_vert.pos_, 1 };
                        vertex.uv_ = src_vert.uv_;
                        vertex.normal_ = glm::normalize(
                            actor_mat3 * src_vert.normal_
                        );

                        dst_pair.mesh_.add_vertex(vertex);
                    }
                }
            }
        }
    }

    void convert_skeleton(
        dal::Skeleton& dst, const dal::SceneIntermediate::Skeleton& src
    ) {
        const auto root_mat = src.root_transform_.make_mat4();

        for (auto& src_joint : src.joints_) {
            auto& dst_joint = dst.joints_.emplace_back();

            dst_joint.name_ = src_joint.name_;
            dst_joint.parent_index_ = dst.find_by_name(src_joint.parent_name_);
            dst_joint.joint_type_ = src_joint.joint_type_;
            dst_joint.offset_mat_ = src_joint.offset_mat_;
        }
    }

}  // namespace


// remove_duplicate_materials
namespace {

    const scene_t::Material* find_material_with_same_physical_properties(
        const scene_t::Material& material,
        const std::vector<scene_t::Material>& list
    ) {
        for (auto& x : list) {
            if (x.is_physically_same(material)) {
                return &x;
            }
        }
        return nullptr;
    }

}  // namespace


// reduce_joints
namespace {

    using str_set_t = std::unordered_set<std::string>;

    ::str_set_t make_set_union(const ::str_set_t& a, const ::str_set_t& b) {
        ::str_set_t output;
        output.insert(a.begin(), a.end());
        output.insert(b.begin(), b.end());
        return output;
    }

    ::str_set_t make_set_intersection(
        const ::str_set_t& a, const ::str_set_t& b
    ) {
        ::str_set_t output;

        auto& smaller_set = a.size() < b.size() ? a : b;
        auto& larger_set = a.size() < b.size() ? b : a;

        for (auto iter = smaller_set.begin(); iter != smaller_set.end();
             ++iter) {
            if (larger_set.end() != larger_set.find(*iter)) {
                output.insert(*iter);
            }
        }

        return output;
    }

    ::str_set_t make_set_difference(
        const ::str_set_t& a, const ::str_set_t& b
    ) {
        ::str_set_t output;

        for (auto iter = a.begin(); iter != a.end(); ++iter) {
            if (b.end() == b.find(*iter)) {
                output.insert(*iter);
            }
        }

        return output;
    }


    class JointParentNameManager {

    private:
        struct JointParentName {
            std::string name_, parent_name_;
        };

    public:
        const std::string NO_PARENT_NAME = "{%{%-1%}%}";

    private:
        std::vector<JointParentName> m_data;
        std::unordered_map<std::string, std::string> m_replace_map;

    public:
        void fill_joints(const scene_t::Skeleton& skeleton) {
            this->m_data.resize(skeleton.joints_.size());

            for (size_t i = 0; i < skeleton.joints_.size(); ++i) {
                auto& joint = skeleton.joints_[i];

                this->m_data[i].name_ = joint.name_;
                if (joint.has_parent())
                    this->m_data[i].parent_name_ = joint.parent_name_;
                else
                    this->m_data[i].parent_name_ = this->NO_PARENT_NAME;

                this->m_replace_map[joint.name_] = joint.name_;
            }
        }

        void remove_joint(const std::string& name) {
            const auto found_index = this->find_by_name(name);
            if (-1 == found_index)
                return;

            const auto parent_of_victim =
                this->m_data[found_index].parent_name_;
            this->m_data.erase(this->m_data.begin() + found_index);

            for (auto& joint : this->m_data) {
                if (joint.parent_name_ == name) {
                    joint.parent_name_ = parent_of_victim;
                }
            }

            for (auto& iter : this->m_replace_map) {
                if (iter.second == name) {
                    iter.second = parent_of_victim;
                }
            }
        }

        void remove_except(const ::str_set_t& survivor_names) {
            const auto names_to_remove = ::make_set_difference(
                this->make_names_set(), survivor_names
            );

            for (auto& name : names_to_remove) {
                this->remove_joint(name);
            }
        }

        ::str_set_t make_names_set() const {
            ::str_set_t output;

            for (auto& joint : this->m_data) {
                output.insert(joint.name_);
            }

            return output;
        }

        const std::string& get_replaced_name(const std::string& name) const {
            if (name == this->NO_PARENT_NAME) {
                return this->NO_PARENT_NAME;
            } else {
                return this->m_replace_map.find(name)->second;
            }
        }

    private:
        dal::jointID_t find_by_name(const std::string& name) {
            if (this->NO_PARENT_NAME == name)
                return -1;

            for (dal::jointID_t i = 0; i < this->m_data.size(); ++i) {
                if (this->m_data[i].name_ == name) {
                    return i;
                }
            }

            return -1;
        }
    };


    bool are_skel_anim_compatible(
        const scene_t::Skeleton& skeleton, const scene_t::Animation& anim
    ) {
        for (auto& joint : skeleton.joints_) {
            if (dal::NULL_JID != anim.find_index_by_name(joint.name_)) {
                return true;
            }
        }
        return false;
    }

    std::vector<const scene_t::Animation*> make_compatible_anim_list(
        const scene_t::Skeleton& skeleton,
        const std::vector<scene_t::Animation>& animations
    ) {
        std::vector<const scene_t::Animation*> output;
        for (auto& anim : animations) {
            if (::are_skel_anim_compatible(skeleton, anim)) {
                output.push_back(&anim);
            }
        }
        return output;
    }

    ::str_set_t get_vital_joint_names(const scene_t::Skeleton& skeleton) {
        // Super parents' children are all vital
        ::str_set_t output, super_parents;

        for (auto& joint : skeleton.joints_) {
            if (joint.is_root()) {
                output.insert(joint.name_);
            } else if (dal::JointType::hair_root == joint.joint_type_ ||
                       dal::JointType::skirt_root == joint.joint_type_) {
                super_parents.insert(joint.name_);
                output.insert(joint.name_);
            } else {
                if (super_parents.end() !=
                    super_parents.find(joint.parent_name_)) {
                    super_parents.insert(joint.name_);
                    output.insert(joint.name_);
                }
            }
        }

        return output;
    }

    ::str_set_t get_joint_names_with_non_identity_transform(
        const scene_t::Skeleton& skeleton,
        const std::vector<scene_t::Animation>& animations
    ) {
        ::str_set_t output;

        if (skeleton.joints_.empty())
            return output;

        // Root nodes
        for (auto& joint : skeleton.joints_) {
            if (joint.is_root()) {
                output.insert(joint.name_);
            }
        }

        // Nodes with keyframes
        for (auto& anim : animations) {
            for (auto& joint : anim.joints_) {
                if (!joint.is_almost_identity(0.01)) {
                    output.insert(joint.name_);
                }
            }
        }

        return output;
    }

    scene_t::Skeleton make_new_skeleton(
        const scene_t::Skeleton& src_skeleton,
        const ::JointParentNameManager& jname_manager
    ) {
        scene_t::Skeleton output;
        output.name_ = src_skeleton.name_;
        output.root_transform_ = src_skeleton.root_transform_;
        const auto survivor_joints = jname_manager.make_names_set();

        for (auto& src_joint : src_skeleton.joints_) {
            if (survivor_joints.end() == survivor_joints.find(src_joint.name_))
                continue;

            const auto& parent_name = src_joint.has_parent()
                                          ? src_joint.parent_name_
                                          : jname_manager.NO_PARENT_NAME;
            const auto& parent_replace_name = jname_manager.get_replaced_name(
                parent_name
            );
            output.joints_.push_back(src_joint);
        }

        for (auto& joint : output.joints_) {
            if (joint.is_root())
                continue;

            const auto& original_parent_name = joint.parent_name_;
            const auto& new_parent_name = jname_manager.get_replaced_name(
                original_parent_name
            );
            if (jname_manager.NO_PARENT_NAME != new_parent_name) {
                joint.parent_name_ = new_parent_name;
            } else {
                joint.parent_name_.clear();
            }
        }

        return output;
    }

    std::unordered_map<dal::jointID_t, dal::jointID_t> make_index_replace_map(
        const scene_t::Skeleton& froskeleton_,
        const scene_t::Skeleton& to_skeleton,
        const JointParentNameManager& jname_manager
    ) {
        std::unordered_map<dal::jointID_t, dal::jointID_t> output;
        output[-1] = -1;

        for (size_t i = 0; i < froskeleton_.joints_.size(); ++i) {
            const auto& froname_ = froskeleton_.joints_[i].name_;
            const auto& to_name = jname_manager.get_replaced_name(froname_);
            const auto to_index = to_skeleton.find_index_by_name(to_name);
            assert(dal::NULL_JID != to_index);
            output[i] = to_index;
        }

        return output;
    }

    std::optional<scene_t::Skeleton> reduce_joints(
        const scene_t::Skeleton& skeleton,
        const std::vector<scene_t::Animation>& animations,
        std::vector<scene_t::Mesh>& meshes
    ) {
        const auto compatible_animations = ::make_compatible_anim_list(
            skeleton, animations
        );
        if (compatible_animations.empty())
            return std::nullopt;

        const auto needed_joint_names = ::make_set_union(
            ::get_joint_names_with_non_identity_transform(skeleton, animations),
            ::get_vital_joint_names(skeleton)
        );

        ::JointParentNameManager joint_parent_names;
        joint_parent_names.fill_joints(skeleton);
        joint_parent_names.remove_except(needed_joint_names);

        const auto new_skeleton = ::make_new_skeleton(
            skeleton, joint_parent_names
        );
        const auto index_replace_map = ::make_index_replace_map(
            skeleton, new_skeleton, joint_parent_names
        );
        const auto max_j_idx = static_cast<dal::jointID_t>(
            new_skeleton.joints_.size()
        );

        for (auto& mesh : meshes) {
            for (auto& vertex : mesh.vertices_) {
                for (int i = 0; i < 4; ++i) {
                    const auto jid = vertex.j_indices_[i];
                    if (dal::NULL_JID == jid)
                        continue;

                    const auto found = index_replace_map.find(jid);
                    assert(index_replace_map.end() != found);

                    const auto new_idx = found->second;
                    assert(-1 <= new_idx);
                    assert(max_j_idx > new_idx);
                    vertex.j_indices_[i] = new_idx;
                }
            }
        }

        return new_skeleton;
    }

}  // namespace


namespace {

    const static std::string TRANSP_SUFFIX = "#transp";


    std::optional<fs::path> find_image_path(
        const std::vector<std::string>& paths, std::string img_name
    ) {
        if (img_name.empty())
            return std::nullopt;

        if (img_name[0] == '/' && img_name[1] == '/')
            img_name = img_name.substr(2);

        const auto img_name_path = fs::u8path(img_name);

        {
            const auto img_path = img_name_path;
            if (fs::is_regular_file(img_path))
                return img_path;
        }

        for (const auto& path : paths) {
            const auto img_path = fs::u8path(path) / img_name_path;
            if (fs::is_regular_file(img_path))
                return img_path;
        }

        return std::nullopt;
    }

    template <typename T>
    std::optional<T> read_file(const fs::path& path) {
        std::ifstream file{ path,
                            std::ios::ate | std::ios::binary | std::ios::in };

        if (!file.is_open())
            return std::nullopt;

        const auto file_size = static_cast<size_t>(file.tellg());
        T buffer;
        buffer.resize(file_size);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        file.close();

        return buffer;
    }


    class MeshSplitterReg {

    public:
        void notify(
            const std::string& mesh_name,
            const scene_t::Mesh& mesh,
            const dal::TImage2D<uint8_t>& img
        ) {
            if (img.channels() != 4)
                return;

            auto& r = this->get_or_create(mesh_name);

            if (0 == r.tri_count_) {
                r.tri_count_ = mesh.indices_.size() / 3;
            } else {
                assert(r.tri_count_ == mesh.indices_.size() / 3);
            }

            const auto wh = glm::vec2(img.width(), img.height());
            const auto tri_count = r.tri_count_;

            for (size_t tri_index = 0; tri_index < tri_count; ++tri_index) {
                const auto& i0 = mesh.indices_[tri_index * 3 + 0];
                const auto& i1 = mesh.indices_[tri_index * 3 + 1];
                const auto& i2 = mesh.indices_[tri_index * 3 + 2];

                const auto& v0 = mesh.vertices_[i0];
                const auto& v1 = mesh.vertices_[i1];
                const auto& v2 = mesh.vertices_[i2];

                const auto uv0 = v0.uv_ * wh;
                const auto uv1 = v1.uv_ * wh;
                const auto uv2 = v2.uv_ * wh;

                if (this->is_transp(uv0, uv1, uv2, img)) {
                    r.transp_tri_indices_.insert(tri_index);
                }
            }

            return;
        }

        void notify(
            const std::string& mesh_name,
            const scene_t::Mesh& mesh,
            dal::KtxImage& img
        ) {
            if (img.esize() != 4)
                return;

            auto& r = this->get_or_create(mesh_name);

            if (0 == r.tri_count_) {
                r.tri_count_ = mesh.indices_.size() / 3;
            } else {
                assert(r.tri_count_ == mesh.indices_.size() / 3);
            }

            const auto wh = glm::vec2(img.base_width(), img.base_height());
            const auto tri_count = r.tri_count_;

            for (size_t tri_index = 0; tri_index < tri_count; ++tri_index) {
                const auto& i0 = mesh.indices_[tri_index * 3 + 0];
                const auto& i1 = mesh.indices_[tri_index * 3 + 1];
                const auto& i2 = mesh.indices_[tri_index * 3 + 2];

                const auto& v0 = mesh.vertices_[i0];
                const auto& v1 = mesh.vertices_[i1];
                const auto& v2 = mesh.vertices_[i2];

                const auto uv0 = v0.uv_ * wh;
                const auto uv1 = v1.uv_ * wh;
                const auto uv2 = v2.uv_ * wh;

                if (this->is_transp(uv0, uv1, uv2, img)) {
                    r.transp_tri_indices_.insert(tri_index);
                }
            }

            return;
        }

        bool is_eligible_for_replacement(const std::string& mesh_name) const {
            const auto found = this->records_.find(mesh_name);
            if (this->records_.end() == found)
                return false;

            const auto& r = found->second;
            if (r.transp_tri_indices_.empty())
                return false;

            return true;
        }

        std::optional<std::pair<scene_t::Mesh, scene_t::Mesh>> try_build(
            const std::string& mesh_name, const scene_t::Mesh& mesh
        ) const {
            const auto found = this->records_.find(mesh_name);
            if (this->records_.end() == found)
                return std::nullopt;

            const auto& r = found->second;
            if (r.transp_tri_indices_.empty())
                return std::nullopt;
            if (r.tri_count_ != mesh.indices_.size() / 3)
                return std::nullopt;

            scene_t::Mesh opaque, transp;
            opaque.name_ = mesh_name;
            transp.name_ = mesh_name + ::TRANSP_SUFFIX;
            opaque.skeleton_name_ = mesh.skeleton_name_;
            transp.skeleton_name_ = mesh.skeleton_name_;

            for (size_t tri_index = 0; tri_index < r.tri_count_; ++tri_index) {
                const auto& i0 = mesh.indices_[tri_index * 3 + 0];
                const auto& i1 = mesh.indices_[tri_index * 3 + 1];
                const auto& i2 = mesh.indices_[tri_index * 3 + 2];

                const auto& v0 = mesh.vertices_[i0];
                const auto& v1 = mesh.vertices_[i1];
                const auto& v2 = mesh.vertices_[i2];

                const auto is_tranp = r.transp_tri_indices_.end() !=
                                      r.transp_tri_indices_.find(tri_index);

                if (is_tranp) {
                    transp.add_vertex(v0);
                    transp.add_vertex(v1);
                    transp.add_vertex(v2);
                } else {
                    opaque.add_vertex(v0);
                    opaque.add_vertex(v1);
                    opaque.add_vertex(v2);
                }
            }

            return std::make_pair(opaque, transp);
        }

    private:
        struct Record {
            std::unordered_set<size_t> transp_tri_indices_;
            size_t tri_count_ = 0;
        };

        Record& get_or_create(const std::string& mesh_name) {
            auto found = this->records_.find(mesh_name);
            if (this->records_.end() != found) {
                return found->second;
            } else {
                return this->records_.emplace(mesh_name, Record{})
                    .first->second;
            }
        }

        static bool is_transp(
            const glm::vec2& tc0,
            const glm::vec2& tc1,
            const glm::vec2& tc2,
            const dal::TImage2D<uint8_t>& img
        ) {
            const sung::Triangle2<float> tri{ dal::vec_cast<float>(tc0),
                                              dal::vec_cast(tc1),
                                              dal::vec_cast(tc2) };

            sung::Aabb2D<float> aabb;
            aabb.set(tc0.x, tc0.y);
            aabb.expand_to_span(tc1.x, tc1.y);
            aabb.expand_to_span(tc2.x, tc2.y);

            const auto x_min = static_cast<int64_t>(std::floor(aabb.x_min()));
            const auto x_max = static_cast<int64_t>(std::ceil(aabb.x_max()));
            const auto y_min = static_cast<int64_t>(std::floor(aabb.y_min()));
            const auto y_max = static_cast<int64_t>(std::ceil(aabb.y_max()));

            for (int64_t x = x_min; x <= x_max; ++x) {
                for (int64_t y = y_min; y <= y_max; ++y) {
                    if (!tri.is_inside_cl({ x + 0.5f, y + 0.5f }))
                        continue;

                    const auto img_u8_ptr = img.texel_ptr(
                        x % img.width(), y % img.height()
                    );
                    if (img_u8_ptr != nullptr) {
                        const auto alpha = img_u8_ptr[3];
                        if (alpha < 254) {
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        static bool is_transp(
            const glm::vec2& tc0,
            const glm::vec2& tc1,
            const glm::vec2& tc2,
            dal::KtxImage& img
        ) {
            const sung::Triangle2<float> tri{ dal::vec_cast<float>(tc0),
                                              dal::vec_cast(tc1),
                                              dal::vec_cast(tc2) };

            sung::Aabb2D<float> aabb;
            aabb.set(tc0.x, tc0.y);
            aabb.expand_to_span(tc1.x, tc1.y);
            aabb.expand_to_span(tc2.x, tc2.y);

            const auto x_min = static_cast<int64_t>(std::floor(aabb.x_min()));
            const auto x_max = static_cast<int64_t>(std::ceil(aabb.x_max()));
            const auto y_min = static_cast<int64_t>(std::floor(aabb.y_min()));
            const auto y_max = static_cast<int64_t>(std::ceil(aabb.y_max()));

            for (int64_t x = x_min; x <= x_max; ++x) {
                for (int64_t y = y_min; y <= y_max; ++y) {
                    if (!tri.is_inside_cl({ x + 0.5f, y + 0.5f }))
                        continue;

                    const auto img_u8_ptr = img.get_base_pixel(
                        x % img.base_width(), y % img.base_height()
                    );
                    if (img_u8_ptr) {
                        const auto alpha = img_u8_ptr->a;
                        if (alpha < 254) {
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        std::unordered_map<std::string, Record> records_;
    };

}  // namespace
namespace dal {

    void split_by_transparency(
        SceneIntermediate& scene, const std::vector<std::string>& tex_lookup
    ) {
        using namespace std;

        const auto mat_count = scene.materials_.size();
        for (size_t i = 0; i < mat_count; ++i) {
            scene.materials_[i].transparency_ = true;
            scene.materials_.push_back(scene.materials_[i]);
            scene.materials_.back().name_ += ::TRANSP_SUFFIX;
            scene.materials_[i].transparency_ = false;
        }
        std::sort(
            scene.materials_.begin(),
            scene.materials_.end(),
            [](const auto& a, const auto& b) { return a.name_ < b.name_; }
        );

        ::MeshSplitterReg splitters;

        for (auto& mesh_actor : scene.mesh_actors_) {
            for (auto& render_pair : mesh_actor.render_pairs_) {
                auto mesh = scene.find_mesh_by_name(render_pair.mesh_name_);
                if (nullptr == mesh)
                    continue;
                auto mat = scene.find_material_by_name(
                    render_pair.material_name_
                );
                if (nullptr == mat)
                    continue;
                auto img_path = ::find_image_path(tex_lookup, mat->albedo_map_);
                if (!img_path)
                    continue;
                auto img_file_content = ::read_file<vector<uint8_t>>(*img_path);
                if (!img_file_content)
                    continue;

                dal::ImageParseInfo pinfo;
                pinfo.file_path_ = img_path.value().u8string();
                pinfo.data_ = img_file_content->data();
                pinfo.size_ = img_file_content->size();
                auto img = dal::parse_img(pinfo);
                if (!img)
                    continue;
                if (!img->is_ready())
                    continue;

                if (auto img_u8 = img->as<dal::TImage2D<uint8_t>>()) {
                    splitters.notify(mesh->name_, *mesh, *img_u8);
                } else if (auto img_ktx = img->as<dal::KtxImage>()) {
                    if (img_ktx->need_transcoding()) {
                        if (img_ktx->transcode(KTX_TTF_RGBA32)) {
                            splitters.notify(mesh->name_, *mesh, *img_ktx);
                        }
                    }
                }
            }
        }

        const auto mesh_count = scene.meshes_.size();
        for (size_t i = 0; i < mesh_count; ++i) {
            auto& mesh = scene.meshes_[i];
            auto new_meshes = splitters.try_build(mesh.name_, mesh);
            if (!new_meshes.has_value()) {
                continue;
            }

            mesh = std::move(new_meshes->first);
            scene.meshes_.push_back(std::move(new_meshes->second));
        }

        for (auto& mesh_actor : scene.mesh_actors_) {
            auto& rpairs = mesh_actor.render_pairs_;
            const auto rpairs_size = rpairs.size();
            for (size_t i = 0; i < rpairs_size; ++i) {
                auto& rpair = rpairs[i];
                if (splitters.is_eligible_for_replacement(rpair.mesh_name_)) {
                    auto& new_rpair = rpairs.emplace_back(rpair);
                    new_rpair.mesh_name_ += ::TRANSP_SUFFIX;
                    new_rpair.material_name_ += ::TRANSP_SUFFIX;

                    assert(scene.find_mesh_by_name(new_rpair.mesh_name_));
                    assert(
                        scene.find_material_by_name(new_rpair.material_name_)
                    );
                }
            }
        }

        return;
    }

}  // namespace dal


namespace dal {

    // Optimize

    void apply_root_transform(SceneIntermediate& scene) {
        const auto& root_m4 = scene.root_transform_;
        const auto root_m4_inv = glm::inverse(root_m4);
        const auto root_m3 = glm::mat3{ root_m4 };

        for (auto& mesh : scene.meshes_) {
            for (auto& vertex : mesh.vertices_) {
                ::apply_transform(root_m4, vertex.pos_);
                vertex.normal_ = glm::normalize(root_m3 * vertex.normal_);
            }
        }

        for (auto& skeleton : scene.skeletons_) {
            for (auto& joint : skeleton.joints_) {
                joint.offset_mat_ = root_m4 * joint.offset_mat_ * root_m4_inv;
            }
        }

        for (auto& animation : scene.animations_) {
            for (auto& joint : animation.joints_) {
                for (auto& x : joint.translations_) {
                    ::apply_transform(root_m4, x.second);
                }

                for (auto& x : joint.rotations_) {
                    ::apply_transform(root_m3, x.second);
                }
            }
        }

        for (auto& mesh_actor : scene.mesh_actors_) {
            ::apply_transform(root_m4, root_m3, mesh_actor.transform_);
        }

        for (auto& light : scene.dlights_) {
            ::apply_transform(root_m4, root_m3, light.transform_);
        }

        for (auto& light : scene.plights_) {
            ::apply_transform(root_m4, root_m3, light.transform_);
        }

        for (auto& light : scene.slights_) {
            ::apply_transform(root_m4, root_m3, light.transform_);
        }

        scene.root_transform_ = glm::mat4{ 1 };
    }

    void reduce_indexed_vertices(SceneIntermediate::Mesh& mesh) {
        scene_t::Mesh builder;
        for (auto& index : mesh.indices_)
            builder.add_vertex(mesh.vertices_[index]);

        std::swap(mesh.vertices_, builder.vertices_);
        std::swap(mesh.indices_, builder.indices_);
    }

    void remove_duplicate_materials(SceneIntermediate& scene) {
        ::StringReplaceMap replace_map;

        {
            std::vector<::scene_t::Material> new_materials;

            for (auto& mat : scene.materials_) {
                const auto found =
                    ::find_material_with_same_physical_properties(
                        mat, new_materials
                    );
                if (nullptr != found) {
                    assert(mat.name_ != found->name_);
                    replace_map.add(mat.name_, found->name_);
                } else {
                    new_materials.push_back(mat);
                }
            }

            std::swap(scene.materials_, new_materials);
        }

        for (auto& mesh_actor : scene.mesh_actors_) {
            for (auto& render_pair : mesh_actor.render_pairs_) {
                render_pair.material_name_ = replace_map.get(
                    render_pair.material_name_
                );
            }
        }
    }

    void reduce_joints(SceneIntermediate& scene) {
        for (auto& skel : scene.skeletons_) {
            const auto result = ::reduce_joints(
                skel, scene.animations_, scene.meshes_
            );
            if (result.has_value()) {
                skel = result.value();
            }
        }
    }

    void merge_redundant_mesh_actors(SceneIntermediate& scene) {
        const auto mesh_actor_count = scene.mesh_actors_.size();

        for (size_t i = 1; i < mesh_actor_count; ++i) {
            auto& prey_actor = scene.mesh_actors_[i];

            for (size_t j = 0; j < i; ++j) {
                auto& dst_actor = scene.mesh_actors_[j];
                if (dst_actor.render_pairs_.empty())
                    continue;

                if (dst_actor.can_merge_with(prey_actor)) {
                    dst_actor.render_pairs_.insert(
                        dst_actor.render_pairs_.end(),
                        prey_actor.render_pairs_.begin(),
                        prey_actor.render_pairs_.end()
                    );
                    prey_actor.render_pairs_.clear();
                }
            }
        }
    }

    void remove_empty_meshes(SceneIntermediate& scene) {
        auto& mesh_actors = scene.mesh_actors_;

        std::unordered_set<std::string> empty_mesh_names;
        for (auto& mesh : scene.meshes_) {
            if (mesh.indices_.empty())
                empty_mesh_names.insert(mesh.name_);
        }

        for (auto& mactor : mesh_actors) {
            mactor.render_pairs_.erase(
                std::remove_if(
                    mactor.render_pairs_.begin(),
                    mactor.render_pairs_.end(),
                    [&empty_mesh_names](const auto& pair) {
                        return empty_mesh_names.end() !=
                               empty_mesh_names.find(pair.mesh_name_);
                    }
                ),
                mactor.render_pairs_.end()
            );
        }

        for (auto it = mesh_actors.begin(); it != mesh_actors.end();) {
            if (!it->render_pairs_.empty()) {
                ++it;
                continue;
            }
            if (!it->transform_.is_identity()) {
                ++it;
                continue;
            }

            const auto& old_parent = it->name_;
            const auto& new_parent = it->parent_name_;
            for (auto& actor : mesh_actors) {
                if (actor.parent_name_ == old_parent) {
                    actor.parent_name_ = new_parent;
                }
            }

            it = mesh_actors.erase(it);
        }

        scene.meshes_.erase(
            std::remove_if(
                scene.meshes_.begin(),
                scene.meshes_.end(),
                [&empty_mesh_names](const auto& mesh) {
                    return empty_mesh_names.end() !=
                           empty_mesh_names.find(mesh.name_);
                }
            ),
            scene.meshes_.end()
        );
    }

    // Modify

    void flip_uv_vertically(SceneIntermediate::Mesh& mesh) {
        for (auto& vertex : mesh.vertices_) {
            vertex.uv_.y = 1.f - vertex.uv_.y;
        }
    }

    void clear_collection_info(SceneIntermediate& scene) {
        for (auto& actor : scene.mesh_actors_) actor.collections_.clear();
        for (auto& actor : scene.dlights_) actor.collections_.clear();
        for (auto& actor : scene.plights_) actor.collections_.clear();
        for (auto& actor : scene.slights_) actor.collections_.clear();
    }

    // Convert

    Model convert_to_model_dmd(const SceneIntermediate& scene) {
        Model output;

        ::convert_meshes(output, scene);

        if (scene.skeletons_.size() > 1) {
            throw std::runtime_error{ "Multiple skeletons are not supported" };
        } else if (scene.skeletons_.size() == 1) {
            ::convert_skeleton(output.skeleton_, scene.skeletons_.back());
        }

        for (auto& src_anim : scene.animations_) {
            output.animations_.push_back(src_anim);
        }

        return output;
    }

}  // namespace dal
