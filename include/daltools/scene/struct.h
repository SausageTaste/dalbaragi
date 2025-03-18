#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>


namespace dal::parser {

    using jointID_t = int32_t;

    constexpr jointID_t NULL_JID = -1;


    enum class JointType {
        basic = 0,
        hair_root = 1,
        skirt_root = 2,
    };


    struct AABB3 {
        glm::vec3 min_, max_;
    };


    struct SceneIntermediate {

    public:
        class Transform {

        public:
            glm::vec3 pos_{ 0, 0, 0 };
            glm::quat quat_{ 1, 0, 0, 0 };
            glm::vec3 scale_{ 1, 1, 1 };

        public:
            bool operator==(const Transform& other) const;

            bool operator!=(const Transform& other) const {
                return !Transform::operator==(other);
            }

            bool is_identity() const;

            glm::mat4 make_mat4() const;
        };


        struct IActor {
            std::string name_;
            std::string parent_name_;
            std::vector<std::string> collections_;
            Transform transform_;
            bool hidden_ = false;
        };


        struct VertexJointPair {
            jointID_t index_ = NULL_JID;
            float weight_ = 0.f;
        };


        struct Vertex {

        public:
            glm::vec3 pos_;
            glm::vec2 uv_;
            glm::vec3 normal_;
            std::vector<VertexJointPair> joints_;

        public:
            bool are_same(const Vertex& other);
        };


        struct Mesh {

        public:
            std::string name_;
            std::string skeleton_name_;
            std::vector<Vertex> vertices_;
            std::vector<size_t> indices_;

        public:
            void add_vertex(const Vertex& vertex);

            void concat(const Mesh& other);
        };


        struct Material {

        public:
            std::string name_;

            float roughness_ = 0.5;
            float metallic_ = 0.0;
            bool transparency_ = false;

            std::string albedo_map_;
            std::string roughness_map_;
            std::string metallic_map_;
            std::string normal_map_;

        public:
            bool operator==(const Material& rhs) const;

            bool is_physically_same(const Material& other) const;
        };


        struct SkelJoint {

        public:
            std::string name_;
            std::string parent_name_;
            JointType joint_type_ = JointType::basic;
            glm::mat4 offset_mat_{ 1 };

        public:
            bool has_parent() const { return !this->parent_name_.empty(); }

            bool is_root() const { return this->parent_name_.empty(); }
        };


        struct Skeleton {

        public:
            std::string name_;
            Transform root_transform_;
            std::vector<SkelJoint> joints_;

        public:
            jointID_t find_index_by_name(const std::string& name) const;
        };


        struct AnimJoint {

        public:
            std::string name_;
            std::vector<std::pair<float, glm::vec3>> translations_;
            std::vector<std::pair<float, glm::quat>> rotations_;
            std::vector<std::pair<float, float>> scales_;

        public:
            void add_position(float time, float x, float y, float z);
            void add_rotation(float time, float w, float x, float y, float z);
            void add_scale(float time, float x);

            float get_max_time_point() const;

            bool is_almost_identity(double epsilon) const;
        };


        struct Animation {

        public:
            std::string name_;
            std::vector<AnimJoint> joints_;
            float ticks_per_sec_ = 1;

        public:
            float calc_duration_in_ticks() const;

            jointID_t find_index_by_name(const std::string& name) const;
        };


        struct RenderPair {
            std::string mesh_name_;
            std::string material_name_;
        };


        struct MeshActor : public IActor {

        public:
            std::vector<RenderPair> render_pairs_;

        public:
            bool can_merge_with(const MeshActor& other) const;
        };


        struct ILight {
            glm::vec3 color_;
            float intensity_ = 1000.f;
            bool has_shadow_ = false;
        };


        struct DirectionalLight
            : public IActor
            , public ILight {};


        struct PointLight
            : public IActor
            , public ILight {
            float max_distance_ = 0.f;
        };


        struct Spotlight : public PointLight {
            float spot_degree_ = 0.f;
            float spot_blend_ = 0.f;
        };


    public:
        std::string name_;
        glm::mat4 root_transform_{ 1 };

        std::vector<Mesh> meshes_;
        std::vector<Material> materials_;
        std::vector<Skeleton> skeletons_;
        std::vector<Animation> animations_;

        std::vector<MeshActor> mesh_actors_;
        std::vector<DirectionalLight> dlights_;
        std::vector<PointLight> plights_;
        std::vector<Spotlight> slights_;

    public:
        Mesh* find_mesh_by_name(const std::string& name);

        const Mesh* find_mesh_by_name(const std::string& name) const;

        const Material* find_material_by_name(const std::string& name) const;

        const Skeleton* find_skeleton_by_name(const std::string& name) const;

        const IActor* find_actor_by_name(const std::string& name) const;

        glm::mat4 make_hierarchy_transform(const IActor& actor) const;
    };


    struct Vertex {
        glm::vec3 pos_;
        glm::vec3 normal_;
        glm::vec2 uv_;

        bool operator==(const Vertex& other) const {
            return this->is_equal(other);
        }

        bool is_equal(const Vertex& other) const;
    };


    struct VertexJoint {
        glm::ivec4 joint_indices_;
        glm::vec4 joint_weights_;
        glm::vec3 pos_;
        glm::vec3 normal_;
        glm::vec2 uv_;

        bool operator==(const VertexJoint& other) const {
            return this->is_equal(other);
        }

        bool is_equal(const VertexJoint& other) const;
    };

    static_assert(
        sizeof(VertexJoint::joint_indices_) ==
        sizeof(VertexJoint::joint_weights_)
    );
    constexpr int NUM_JOINTS_PER_VERTEX = sizeof(VertexJoint::joint_indices_) /
                                          sizeof(float);


    using Material = SceneIntermediate::Material;


    struct Mesh_Straight {
        std::vector<float> vertices_, uv_coordinates_, normals_;

        void concat(const Mesh_Straight& other);
    };


    struct Mesh_StraightJoint : public Mesh_Straight {
        std::vector<float> joint_weights_;
        std::vector<jointID_t> joint_indices_;

        void concat(const Mesh_StraightJoint& other);
    };


    template <typename _Vertex>
    struct TMesh_Indexed {
        std::vector<_Vertex> vertices_;
        std::vector<uint32_t> indices_;

        using VERT_TYPE = _Vertex;

        void add_vertex(const _Vertex& vert) {
            for (size_t i = 0; i < vertices_.size(); ++i) {
                if (vert == vertices_[i]) {
                    indices_.push_back(i);
                    return;
                }
            }

            this->append_vertex(vert);
        }

        void append_vertex(const _Vertex& vert) {
            indices_.push_back(vertices_.size());
            vertices_.push_back(vert);
        }

        void concat(const TMesh_Indexed<_Vertex>& other) {
            for (const auto index : other.indices_) {
                this->add_vertex(other.vertices_[index]);
            }
        }
    };

    using Mesh_Indexed = TMesh_Indexed<Vertex>;

    using Mesh_IndexedJoint = TMesh_Indexed<VertexJoint>;


    template <typename _Mesh>
    struct RenderUnit {
        std::string name_;
        _Mesh mesh_;
        Material material_;
    };


    struct SkelJoint {
        std::string name_;
        jointID_t parent_index_;
        JointType joint_type_;
        glm::mat4 offset_mat_;
    };


    struct Skeleton {
        glm::mat4 root_transform_{ 1 };
        std::vector<SkelJoint> joints_;

        // Returns -1 if not found
        jointID_t find_by_name(const std::string& name) const;
    };


    using AnimJoint = SceneIntermediate::AnimJoint;
    using Animation = SceneIntermediate::Animation;


    struct Model {
        std::vector<RenderUnit<Mesh_Straight>> units_straight_;
        std::vector<RenderUnit<Mesh_StraightJoint>> units_straight_joint_;
        std::vector<RenderUnit<Mesh_Indexed>> units_indexed_;
        std::vector<RenderUnit<Mesh_IndexedJoint>> units_indexed_joint_;

        std::vector<Animation> animations_;
        Skeleton skeleton_;
        AABB3 aabb_;
    };

}  // namespace dal::parser
