#pragma once

#include "dal/scene/struct.hpp"


namespace dal {

    void optimize_vertex_cache(dal::SceneIntermediate::Mesh& mesh);
    void optimize_vertex_overdraw(dal::SceneIntermediate::Mesh& mesh);
    void optimize_vertex_fetch(dal::SceneIntermediate::Mesh& mesh);

}  // namespace dal
