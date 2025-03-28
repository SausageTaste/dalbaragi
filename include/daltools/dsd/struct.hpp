#pragma once

#include <climits>
#include <optional>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>


namespace dal::dsd {

    static_assert(CHAR_BIT == 8);

    using VtxIndex = uint32_t;


    template <typename T>
    struct DataRange {

    public:
        constexpr uint64_t stride() const { return sizeof(T); }

        constexpr uint64_t byte_size() const { return item_count_ * sizeof(T); }

        constexpr uint64_t end_byte_offset() const {
            return byte_offset_ + this->byte_size();
        }

        T* resolve(void* base) const {
            return reinterpret_cast<T*>(
                static_cast<uint8_t*>(base) + byte_offset_
            );
        }

        T* resolve(const void* base) const {
            return reinterpret_cast<const T*>(
                static_cast<const uint8_t*>(base) + byte_offset_
            );
        }

    public:
        uint64_t byte_offset_ = 0;
        uint64_t item_count_ = 0;
    };


    struct Vertex {
        glm::vec3 pos_;
        glm::vec3 normal_;
        glm::vec4 tangent_;
        glm::vec2 texco0_;
        glm::vec2 texco1_;
    };


    struct Mesh {
        DataRange<Vertex> vtx_;
        DataRange<VtxIndex> idx_;
    };


    struct Material {
        DataRange<char> name_;
        DataRange<char> albedo_map_;
        DataRange<char> normal_map_;
        DataRange<char> orm_map_;
        float roughness_ = 0;
        float metallic_ = 0;
    };


    struct Transform {
        glm::dvec3 pos_;
        glm::quat rot_;
        glm::vec3 scale_;
    };


    struct DrawItem {
        uint64_t mesh_idx_ = 0;
        uint64_t material_idx_ = 0;
        Transform transform_;
    };


    enum class ColliderType {
        obb,
    };


    struct ColObb {
        Transform transform_;
        glm::vec3 min_;
        glm::vec3 max_;
    };


    struct MeshActor {
        DataRange<char> name_;
        DataRange<char> parent_;
        DataRange<DrawItem> draw_items_;
        Transform transform_;
    };


    class Header {

    public:
        bool is_magic_valid() const noexcept;
        void set_magic();

    public:
        uint32_t magic_ = 0;
        uint32_t version_ = 1;
        glm::dmat4 transform_{ 1 };
        DataRange<char> name_;
        DataRange<char> created_datetime_;

        DataRange<Mesh> meshes_;
        DataRange<Material> materials_;
        DataRange<MeshActor> mesh_actors_;
    };

}  // namespace dal::dsd
