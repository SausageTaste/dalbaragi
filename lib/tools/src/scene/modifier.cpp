#include "daltools/scene/modifier.h"

#include <unordered_map>
#include <unordered_set>


namespace {

    namespace dalp = dal::parser;


    void fill_mesh_skinned(
        dalp::Mesh_IndexedJoint& output, const dalp::Mesh_StraightJoint& input
    ) {
        const auto vertex_count = input.vertices_.size() / 3;

        for (size_t i = 0; i < vertex_count; ++i) {
            dalp::VertexJoint vert;

            vert.pos_ = glm::vec3{ input.vertices_[3 * i + 0],
                                   input.vertices_[3 * i + 1],
                                   input.vertices_[3 * i + 2] };

            vert.uv_ = glm::vec2{ input.uv_coordinates_[2 * i + 0],
                                  input.uv_coordinates_[2 * i + 1] };

            vert.normal_ = glm::normalize(glm::vec3{ input.normals_[3 * i + 0],
                                                     input.normals_[3 * i + 1],
                                                     input.normals_[3 * i + 2] }
            );

            static_assert(4 == dalp::NUM_JOINTS_PER_VERTEX);

            vert.joint_weights_ = glm::vec4{
                input.joint_weights_[dalp::NUM_JOINTS_PER_VERTEX * i + 0],
                input.joint_weights_[dalp::NUM_JOINTS_PER_VERTEX * i + 1],
                input.joint_weights_[dalp::NUM_JOINTS_PER_VERTEX * i + 2],
                input.joint_weights_[dalp::NUM_JOINTS_PER_VERTEX * i + 3]
            };

            vert.joint_indices_ = glm::ivec4{
                input.joint_indices_[dalp::NUM_JOINTS_PER_VERTEX * i + 0],
                input.joint_indices_[dalp::NUM_JOINTS_PER_VERTEX * i + 1],
                input.joint_indices_[dalp::NUM_JOINTS_PER_VERTEX * i + 2],
                input.joint_indices_[dalp::NUM_JOINTS_PER_VERTEX * i + 3]
            };

            output.add_vertex(vert);
        }
    }

    void fill_mesh_basic(
        dalp::Mesh_Indexed& output, const dalp::Mesh_Straight& input
    ) {
        const auto vertex_count = input.vertices_.size() / 3;

        for (size_t i = 0; i < vertex_count; ++i) {
            dalp::Vertex vert;

            vert.pos_ = glm::vec3{ input.vertices_[3 * i + 0],
                                   input.vertices_[3 * i + 1],
                                   input.vertices_[3 * i + 2] };

            vert.uv_ = glm::vec2{ input.uv_coordinates_[2 * i + 0],
                                  input.uv_coordinates_[2 * i + 1] };

            vert.normal_ = glm::normalize(glm::vec3{ input.normals_[3 * i + 0],
                                                     input.normals_[3 * i + 1],
                                                     input.normals_[3 * i + 2] }
            );

            output.add_vertex(vert);
        }
    }


    template <typename _Mesh>
    dalp::RenderUnit<_Mesh>* find_same_material(
        const dalp::RenderUnit<_Mesh>& criteria,
        std::vector<dalp::RenderUnit<_Mesh>>& units
    ) {
        for (auto& x : units)
            if (x.material_.is_physically_same(criteria.material_))
                return &x;

        return nullptr;
    };

    template <typename _Mesh>
    std::vector<dalp::RenderUnit<_Mesh>> merge_by_material(
        const std::vector<dalp::RenderUnit<_Mesh>>& units
    ) {
        std::vector<dalp::RenderUnit<_Mesh>> output;
        if (units.empty())
            return output;

        output.push_back(units[0]);

        for (size_t i = 1; i < units.size(); ++i) {
            const auto& this_unit = units[i];

            if (this_unit.material_.transparency_) {
                output.push_back(this_unit);
                continue;
            }

            auto dst_unit = ::find_same_material(this_unit, output);

            if (nullptr != dst_unit)
                dst_unit->mesh_.concat(this_unit.mesh_);
            else
                output.push_back(this_unit);
        }

        return output;
    }

}  // namespace


// For reduce_joints
namespace {

    using dalp::jointID_t;
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


    bool is_joint_useless(const dalp::AnimJoint& joint) {
        if (!joint.translations_.empty())
            return false;
        else if (!joint.rotations_.empty())
            return false;
        else if (!joint.scales_.empty())
            return false;
        else
            return true;
    }

    ::str_set_t get_vital_joint_names(const dalp::Skeleton& skeleton) {
        // Super parents' children are all vital
        ::str_set_t output, super_parents;

        for (auto& joint : skeleton.joints_) {
            if (-1 == joint.parent_index_) {
                output.insert(joint.name_);
            } else if (dalp::JointType::hair_root == joint.joint_type_ ||
                       dalp::JointType::skirt_root == joint.joint_type_) {
                super_parents.insert(joint.name_);
                output.insert(joint.name_);
            } else {
                auto& parent = skeleton.joints_.at(joint.parent_index_);
                auto& parent_name = parent.name_;

                if (super_parents.end() != super_parents.find(parent_name)) {
                    super_parents.insert(joint.name_);
                    output.insert(joint.name_);
                }
            }
        }

        return output;
    }

    ::str_set_t get_joint_names_with_non_identity_transform(
        const std::vector<dalp::Animation>& animations,
        const dalp::Skeleton& skeleton
    ) {
        ::str_set_t output;

        if (skeleton.joints_.empty())
            return output;

        // Root nodes
        for (auto& joint : skeleton.joints_) {
            if (-1 == joint.parent_index_) {
                output.insert(joint.name_);
            }
        }

        // Nodes with keyframes
        for (auto& anim : animations) {
            for (auto& joint : anim.joints_) {
                if (!::is_joint_useless(joint)) {
                    output.insert(joint.name_);
                }
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
        void fill_joints(const dalp::Skeleton& skeleton) {
            m_data.resize(skeleton.joints_.size());

            for (size_t i = 0; i < skeleton.joints_.size(); ++i) {
                auto& joint = skeleton.joints_[i];

                m_data[i].name_ = joint.name_;
                if (-1 != joint.parent_index_)
                    m_data[i].parent_name_ =
                        skeleton.joints_[joint.parent_index_].name_;
                else
                    m_data[i].parent_name_ = NO_PARENT_NAME;

                m_replace_map[joint.name_] = joint.name_;
            }
        }

        void remove_joint(const std::string& name) {
            const auto found_index = this->find_by_name(name);
            if (-1 == found_index)
                return;

            const auto parent_of_victim = m_data[found_index].parent_name_;
            m_data.erase(m_data.begin() + found_index);

            for (auto& joint : m_data) {
                if (joint.parent_name_ == name) {
                    joint.parent_name_ = parent_of_victim;
                }
            }

            for (auto& iter : m_replace_map) {
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

            for (auto& joint : m_data) {
                output.insert(joint.name_);
            }

            return output;
        }

        auto& get_replaced_name(const std::string& name) const {
            if (name == NO_PARENT_NAME) {
                return NO_PARENT_NAME;
            } else {
                return m_replace_map.find(name)->second;
            }
        }

    private:
        jointID_t find_by_name(const std::string& name) {
            if (NO_PARENT_NAME == name)
                return -1;

            for (jointID_t i = 0; i < m_data.size(); ++i) {
                if (m_data[i].name_ == name) {
                    return i;
                }
            }

            return -1;
        }
    };

    dalp::Skeleton make_new_skeleton(
        const dalp::Skeleton& src_skeleton,
        const JointParentNameManager& jname_manager
    ) {
        dalp::Skeleton output;
        const auto survivor_joints = jname_manager.make_names_set();

        for (auto& src_joint : src_skeleton.joints_) {
            if (survivor_joints.end() == survivor_joints.find(src_joint.name_))
                continue;

            const std::string& parent_name =
                (-1 != src_joint.parent_index_)
                    ? src_skeleton.joints_[src_joint.parent_index_].name_
                    : jname_manager.NO_PARENT_NAME;
            const auto& parent_replace_name = jname_manager.get_replaced_name(
                parent_name
            );
            output.joints_.push_back(src_joint);
        }

        for (auto& joint : output.joints_) {
            if (-1 == joint.parent_index_)
                continue;

            auto& ori_parent = src_skeleton.joints_[joint.parent_index_];
            const auto& new_parent_name = jname_manager.get_replaced_name(
                ori_parent.name_
            );
            if (jname_manager.NO_PARENT_NAME != new_parent_name) {
                joint.parent_index_ = output.find_by_name(new_parent_name);
            } else {
                joint.parent_index_ = -1;
            }
        }

        return output;
    }

    std::unordered_map<jointID_t, jointID_t> make_index_replace_map(
        const dalp::Skeleton& froskeleton_,
        const dalp::Skeleton& to_skeleton,
        const JointParentNameManager& jname_manager
    ) {
        std::unordered_map<jointID_t, jointID_t> output;
        output[-1] = -1;

        for (size_t i = 0; i < froskeleton_.joints_.size(); ++i) {
            const auto& froname_ = froskeleton_.joints_[i].name_;
            const auto& to_name = jname_manager.get_replaced_name(froname_);
            const auto to_index = to_skeleton.find_by_name(to_name);
            assert(-1 != to_index);
            output[i] = to_index;
        }

        return output;
    }

}  // namespace


namespace dal::parser {

    Mesh_Indexed convert_to_indexed(const Mesh_Straight& input) {
        Mesh_Indexed output;

        const auto vertex_count = input.vertices_.size() / 3;
        assert(2 * vertex_count == input.uv_coordinates_.size());
        assert(3 * vertex_count == input.normals_.size());

        ::fill_mesh_basic(output, input);

        return output;
    }

    Mesh_IndexedJoint convert_to_indexed(const Mesh_StraightJoint& input) {
        Mesh_IndexedJoint output;

        const auto vertex_count = input.vertices_.size() / 3;
        assert(2 * vertex_count == input.uv_coordinates_.size());
        assert(3 * vertex_count == input.normals_.size());
        assert(
            dalp::NUM_JOINTS_PER_VERTEX * vertex_count ==
            input.joint_indices_.size()
        );
        assert(
            dalp::NUM_JOINTS_PER_VERTEX * vertex_count ==
            input.joint_weights_.size()
        );

        ::fill_mesh_skinned(output, input);

        return output;
    }


    std::vector<RenderUnit<Mesh_Straight>> merge_by_material(
        const std::vector<RenderUnit<Mesh_Straight>>& units
    ) {
        return ::merge_by_material(units);
    }

    std::vector<RenderUnit<Mesh_StraightJoint>> merge_by_material(
        const std::vector<RenderUnit<Mesh_StraightJoint>>& units
    ) {
        return ::merge_by_material(units);
    }

    std::vector<RenderUnit<Mesh_Indexed>> merge_by_material(
        const std::vector<RenderUnit<Mesh_Indexed>>& units
    ) {
        return ::merge_by_material(units);
    }

    std::vector<RenderUnit<Mesh_IndexedJoint>> merge_by_material(
        const std::vector<RenderUnit<Mesh_IndexedJoint>>& units
    ) {
        return ::merge_by_material(units);
    }


    JointReductionResult reduce_joints(dalp::Model& model) {
        if (model.animations_.empty())
            return JointReductionResult::needless;

        const auto needed_joint_names = ::make_set_union(
            ::get_joint_names_with_non_identity_transform(
                model.animations_, model.skeleton_
            ),
            ::get_vital_joint_names(model.skeleton_)
        );

        ::JointParentNameManager joint_parent_names;
        joint_parent_names.fill_joints(model.skeleton_);
        joint_parent_names.remove_except(needed_joint_names);

        const auto new_skeleton = ::make_new_skeleton(
            model.skeleton_, joint_parent_names
        );
        const auto index_replace_map = ::make_index_replace_map(
            model.skeleton_, new_skeleton, joint_parent_names
        );

        for (auto& unit : model.units_indexed_joint_) {
            for (auto& vert : unit.mesh_.vertices_) {
                for (size_t i = 0; i < 4; ++i) {
                    const auto new_index =
                        index_replace_map.find(vert.joint_indices_[i])->second;
                    assert(
                        -1 <= new_index &&
                        new_index <
                            static_cast<int64_t>(new_skeleton.joints_.size())
                    );
                    vert.joint_indices_[i] = new_index;
                }
            }
        }

        for (auto& unit : model.units_straight_joint_) {
            for (auto& index : unit.mesh_.joint_indices_) {
                const auto new_index = index_replace_map.find(index)->second;
                assert(
                    -1 <= new_index &&
                    new_index <
                        static_cast<int64_t>(new_skeleton.joints_.size())
                );
                index = new_index;
            }
        }

        model.skeleton_ = new_skeleton;

        return JointReductionResult::success;
    }

}  // namespace dal::parser
