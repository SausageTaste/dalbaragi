#include "dal/scene/mesh_opt.hpp"

#include <meshoptimizer.h>


namespace dal {

    void optimize_vertex_cache(dal::SceneIntermediate::Mesh& mesh) {
        std::vector<size_t> new_indices(mesh.indices_.size());
        meshopt_optimizeVertexCache(
            new_indices.data(),
            mesh.indices_.data(),
            mesh.indices_.size(),
            mesh.vertices_.size()
        );
        std::swap(mesh.indices_, new_indices);
    }

    void optimize_vertex_overdraw(dal::SceneIntermediate::Mesh& mesh) {
        meshopt_optimizeOverdraw(
            mesh.indices_.data(),
            mesh.indices_.data(),
            mesh.indices_.size(),
            &mesh.vertices_[0].pos_.x,
            mesh.vertices_.size(),
            sizeof(dal::SceneIntermediate::Vertex),
            1.05f
        );
    }

    void optimize_vertex_fetch(dal::SceneIntermediate::Mesh& mesh) {
        std::vector<dal::SceneIntermediate::Vertex> vertices(
            mesh.vertices_.size()
        );

        const auto resulting_vert_count = meshopt_optimizeVertexFetch(
            vertices.data(),
            mesh.indices_.data(),
            mesh.indices_.size(),
            mesh.vertices_.data(),
            mesh.vertices_.size(),
            sizeof(dal::SceneIntermediate::Vertex)
        );

        vertices.resize(resulting_vert_count);
        std::swap(mesh.vertices_, vertices);
    }

}  // namespace dal
