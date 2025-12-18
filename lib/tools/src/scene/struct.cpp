#include "daltools/scene/struct.h"

#include <stdexcept>


namespace {

    using scene_t = dal::SceneIntermediate;


    bool are_similar(float lhs, float rhs, float epsilon) {
        return std::abs(lhs - rhs) <= epsilon;
    }

    bool are_similar(const glm::vec2& lhs, const glm::vec2& rhs, float eps) {
        if (!are_similar(lhs.x, rhs.x, eps))
            return false;
        if (!are_similar(lhs.y, rhs.y, eps))
            return false;

        return true;
    }

    bool are_similar(const glm::vec3& lhs, const glm::vec3& rhs, float eps) {
        if (!are_similar(lhs.x, rhs.x, eps))
            return false;
        if (!are_similar(lhs.y, rhs.y, eps))
            return false;
        if (!are_similar(lhs.z, rhs.z, eps))
            return false;

        return true;
    }

    bool are_similar(const glm::vec4& lhs, const glm::vec4& rhs, float eps) {
        if (!are_similar(lhs.x, rhs.x, eps))
            return false;
        if (!are_similar(lhs.y, rhs.y, eps))
            return false;
        if (!are_similar(lhs.z, rhs.z, eps))
            return false;
        if (!are_similar(lhs.w, rhs.w, eps))
            return false;

        return true;
    }

}  // namespace


namespace dal {

    bool Vertex::is_equal(const Vertex& other) const {
        return (
            this->pos_ == other.pos_ && this->uv_ == other.uv_ &&
            this->normal_ == other.normal_
        );
    }

    bool VertexJoint::is_equal(const VertexJoint& other) const {
        return (
            this->pos_ == other.pos_ && this->uv_ == other.uv_ &&
            this->normal_ == other.normal_ &&
            this->joint_weights_ == other.joint_weights_ &&
            this->joint_indices_ == other.joint_indices_
        );
    }


    void Mesh_Straight::concat(const Mesh_Straight& other) {
        this->vertices_.insert(
            vertices_.end(), other.vertices_.begin(), other.vertices_.end()
        );
        this->uv_coordinates_.insert(
            uv_coordinates_.end(),
            other.uv_coordinates_.begin(),
            other.uv_coordinates_.end()
        );
        this->normals_.insert(
            normals_.end(), other.normals_.begin(), other.normals_.end()
        );
    }

    void Mesh_StraightJoint::concat(const Mesh_StraightJoint& other) {
        this->Mesh_Straight::concat(other);

        this->joint_weights_.insert(
            joint_weights_.end(),
            other.joint_weights_.begin(),
            other.joint_weights_.end()
        );
        this->joint_indices_.insert(
            joint_indices_.end(),
            other.joint_indices_.begin(),
            other.joint_indices_.end()
        );
    }


    jointID_t Skeleton::find_by_name(const std::string& name) const {
        const auto size = static_cast<jointID_t>(this->joints_.size());
        for (jointID_t i = 0; i < size; ++i) {
            if (this->joints_[i].name_ == name) {
                return i;
            }
        }

        return -1;
    }

}  // namespace dal


// Vertex
namespace dal {

    bool scene_t::Vertex::are_same(const scene_t::Vertex& other) const {
        if (this->j_indices_ != other.j_indices_)
            return false;
        if (this->j_weights_ != other.j_weights_)
            return false;
        if (this->pos_ != other.pos_)
            return false;
        if (this->normal_ != other.normal_)
            return false;
        if (this->uv_ != other.uv_)
            return false;

        return true;
    }

    bool scene_t::Vertex::are_similar(
        const scene_t::Vertex& rhs, float eps
    ) const {
        if (j_indices_ != rhs.j_indices_)
            return false;
        if (!::are_similar(j_weights_, rhs.j_weights_, eps))
            return false;
        if (!::are_similar(pos_, rhs.pos_, eps))
            return false;
        if (!::are_similar(normal_, rhs.normal_, eps))
            return false;
        if (!::are_similar(uv_, rhs.uv_, eps))
            return false;

        return true;
    }

    /*
    void scene_t::Vertex::add_joint(jointID_t index, float weight) {
        for (int i = 0; i < 4; ++i) {
            if (j_indices_[i] == NULL_JID) {
                j_indices_[i] = index;
                j_weights_[i] = weight;
                return;
            }
        }

        throw std::runtime_error{ "Too many joints on vertex" };
    }
    */

}  // namespace dal


//
namespace dal {

    bool scene_t::Transform::operator==(const scene_t::Transform& other) const {
        if (this->pos_ != other.pos_)
            return false;
        if (this->quat_ != other.quat_)
            return false;
        if (this->scale_ != other.scale_)
            return false;

        return true;
    }

    bool scene_t::Transform::is_identity() const {
        return this->pos_ == glm::vec3{ 0, 0, 0 } &&
               this->quat_ == glm::quat{ 1, 0, 0, 0 } &&
               this->scale_ == glm::vec3{ 1, 1, 1 };
    }


    glm::mat4 scene_t::Transform::make_mat4() const {
        const auto scale = glm::scale(glm::mat4{ 1 }, this->scale_);
        const auto rotation = glm::mat4_cast(this->quat_);
        const auto translation = glm::translate(glm::mat4{ 1 }, this->pos_);
        return translation * rotation * scale;
    }


    void scene_t::Mesh::add_vertex(const scene_t::Vertex& vertex) {
        const auto vert_size = this->vertices_.size();
        for (size_t i = 0; i < vert_size; ++i) {
            if (this->vertices_[i].are_similar(vertex)) {
                this->indices_.push_back(i);
                return;
            }
        }

        this->indices_.push_back(this->vertices_.size());
        this->vertices_.push_back(vertex);
    }

    void scene_t::Mesh::concat(const scene_t::Mesh& other) {
        if (this->skeleton_name_ != other.skeleton_name_)
            throw std::runtime_error{
                "Cannot concatenate meshes with different skeletons"
            };

        for (const auto index : other.indices_) {
            this->add_vertex(other.vertices_[index]);
        }
    }


    bool scene_t::Material::operator==(const scene_t::Material& rhs) const {
        if (!this->is_physically_same(rhs))
            return false;
        if (this->name_ != rhs.name_)
            return false;

        return true;
    }

    bool scene_t::Material::is_physically_same(
        const scene_t::Material& other
    ) const {
        if (this->roughness_ != other.roughness_)
            return false;
        if (this->metallic_ != other.metallic_)
            return false;
        if (this->transparency_ != other.transparency_)
            return false;

        if (this->albedo_map_ != other.albedo_map_)
            return false;
        if (this->roughness_map_ != other.roughness_map_)
            return false;
        if (this->metallic_map_ != other.metallic_map_)
            return false;
        if (this->normal_map_ != other.normal_map_)
            return false;

        return true;
    }


    jointID_t scene_t::Skeleton::find_index_by_name(
        const std::string& name
    ) const {
        const auto size = static_cast<jointID_t>(this->joints_.size());
        for (jointID_t i = 0; i < size; ++i) {
            if (this->joints_[i].name_ == name) {
                return i;
            }
        }
        return NULL_JID;
    }


    void scene_t::AnimJoint::add_position(
        float time, float x, float y, float z
    ) {
        auto& added = this->translations_.emplace_back();

        added.first = time;
        added.second = glm::vec3{ x, y, z };
    }

    void scene_t::AnimJoint::add_rotation(
        float time, float w, float x, float y, float z
    ) {
        auto& added = this->rotations_.emplace_back();

        added.first = time;
        added.second = glm::quat{ w, x, y, z };
    }

    void scene_t::AnimJoint::add_scale(float time, float x) {
        auto& added = this->scales_.emplace_back();

        added.first = time;
        added.second = x;
    }

    float scene_t::AnimJoint::get_max_time_point() const {
        float max_value = 0;

        for (auto& x : this->translations_)
            max_value = glm::max(max_value, x.first);
        for (auto& x : this->rotations_)
            max_value = glm::max(max_value, x.first);
        for (auto& x : this->scales_) max_value = glm::max(max_value, x.first);

        return max_value;
    }

    bool scene_t::AnimJoint::is_almost_identity(double epsilon) const {
        const auto epsilon_sq = epsilon * epsilon;

        for (auto& x : this->translations_) {
            if (glm::dot(x.second, x.second) > epsilon_sq)
                return false;
        }

        for (auto& x : this->rotations_) {
            if (std::abs(x.second.w - 1.f) > epsilon)
                return false;
            if (std::abs(x.second.x) > epsilon)
                return false;
            if (std::abs(x.second.y) > epsilon)
                return false;
            if (std::abs(x.second.z) > epsilon)
                return false;
        }

        for (auto& x : this->scales_) {
            if (std::abs(x.second - 1.f) > epsilon)
                return false;
        }

        return true;
    }


    float scene_t::Animation::calc_duration_in_ticks() const {
        float max_value = 0.f;

        for (auto& joint : this->joints_)
            max_value = glm::max(max_value, joint.get_max_time_point());

        return 0.f != max_value ? max_value : 1.f;
    }

    jointID_t scene_t::Animation::find_index_by_name(
        const std::string& name
    ) const {
        const auto size = static_cast<jointID_t>(this->joints_.size());
        for (jointID_t i = 0; i < size; ++i) {
            if (this->joints_[i].name_ == name) {
                return i;
            }
        }
        return NULL_JID;
    }


    bool scene_t::MeshActor::can_merge_with(const MeshActor& other) const {
        if (this->parent_name_ != other.parent_name_)
            return false;
        if (this->collections_ != other.collections_)
            return false;
        if (this->transform_ != other.transform_)
            return false;
        if (this->hidden_ != other.hidden_)
            return false;

        return true;
    }

}  // namespace dal


// SceneIntermediate
namespace dal {

    scene_t::Mesh* scene_t::find_mesh_by_name(const std::string& name) {
        for (auto& mesh : this->meshes_) {
            if (mesh.name_ == name) {
                return &mesh;
            }
        }

        return nullptr;
    }

    const scene_t::Mesh* scene_t::find_mesh_by_name(
        const std::string& name
    ) const {
        for (auto& mesh : this->meshes_) {
            if (mesh.name_ == name) {
                return &mesh;
            }
        }

        return nullptr;
    }

    const scene_t::Material* scene_t::find_material_by_name(
        const std::string& name
    ) const {
        for (auto& x : this->materials_) {
            if (x.name_ == name) {
                return &x;
            }
        }

        return nullptr;
    }

    const scene_t::Skeleton* scene_t::find_skeleton_by_name(
        const std::string& name
    ) const {
        for (auto& x : this->skeletons_) {
            if (x.name_ == name) {
                return &x;
            }
        }

        return nullptr;
    }

    const scene_t::IActor* scene_t::find_actor_by_name(
        const std::string& name
    ) const {
        for (auto& x : this->mesh_actors_)
            if (x.name_ == name)
                return &x;

        for (auto& x : this->dlights_)
            if (x.name_ == name)
                return &x;

        for (auto& x : this->plights_)
            if (x.name_ == name)
                return &x;

        for (auto& x : this->slights_)
            if (x.name_ == name)
                return &x;

        return nullptr;
    }

    glm::mat4 scene_t::make_hierarchy_transform(
        const scene_t::IActor& actor
    ) const {
        glm::mat4 output = actor.transform_.make_mat4();
        const IActor* cur_actor = &actor;

        while (!cur_actor->parent_name_.empty()) {
            const auto& parent_name = cur_actor->parent_name_;
            cur_actor = this->find_actor_by_name(parent_name);
            if (nullptr == cur_actor) {
                if (nullptr != this->find_skeleton_by_name(parent_name)) {
                    break;
                } else {
                    throw std::runtime_error{
                        "Failed to find a parent actor \"" + parent_name + '"'
                    };
                }
            }

            output = cur_actor->transform_.make_mat4() * output;
        }

        return output;
    }

}  // namespace dal
