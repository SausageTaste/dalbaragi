#pragma once

#include <vector>

#include <sung/basic/bytes.hpp>

#include "dal/parser/scene/struct.hpp"
#include "dal/tools/dsd/struct.hpp"


namespace dal::dsd {

    bool convert(
        std::vector<uint8_t>& output, const dal::SceneIntermediate& scene
    );

}  // namespace dal::dsd
