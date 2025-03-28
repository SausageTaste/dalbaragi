#pragma once

#include <vector>

#include <sung/basic/bytes.hpp>

#include "daltools/dsd/struct.hpp"
#include "daltools/scene/struct.h"


namespace dal::dsd {

    bool convert(
        std::vector<uint8_t>& output,
        const dal::parser::SceneIntermediate& scene
    );

}  // namespace dal::dsd
