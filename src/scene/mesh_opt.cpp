#include "daltools/scene/mesh_opt.hpp"

#include <meshoptimizer.h>


namespace {

    namespace dalp = dal::parser;

}


namespace dal {

    void optimize_vertex_cache(dalp::SceneIntermediate::Mesh& mesh) {
        std::vector<size_t> new_indices(mesh.indices_.size());
        meshopt_optimizeVertexCache(
            new_indices.data(),
            mesh.indices_.data(),
            mesh.indices_.size(),
            mesh.vertices_.size()
        );
        std::swap(mesh.indices_, new_indices);
    }

}  // namespace dal
