#pragma once

#include "daltools/scene/struct.h"


namespace dal {

    void optimize_vertex_cache(dal::parser::SceneIntermediate::Mesh& mesh);
    void optimize_vertex_overdraw(dal::parser::SceneIntermediate::Mesh& mesh);
    void optimize_vertex_fetch(dal::parser::SceneIntermediate::Mesh& mesh);

}  // namespace dal
