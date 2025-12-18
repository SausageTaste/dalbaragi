#pragma once

#include <vector>

#include <sung/basic/bytes.hpp>

#include "dal/dsd/struct.hpp"
#include "dal/scene/struct.hpp"


namespace dal::dsd {

    bool convert(
        std::vector<uint8_t>& output, const dal::SceneIntermediate& scene
    );

}  // namespace dal::dsd
