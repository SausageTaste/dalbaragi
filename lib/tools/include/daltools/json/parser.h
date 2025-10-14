#pragma once

#include <optional>

#include "daltools/common/bin_data.h"
#include "daltools/scene/struct.h"


namespace dal::parser {

    enum class JsonParseResult {
        success,
    };

    JsonParseResult parse_json(
        std::vector<SceneIntermediate>& scenes,
        const uint8_t* const file_content,
        const size_t content_size
    );

    JsonParseResult parse_json(
        std::vector<SceneIntermediate>& scenes, const BinDataView& json_src
    );

    JsonParseResult parse_json_bin(
        std::vector<SceneIntermediate>& scenes,
        const uint8_t* const json_data,
        const size_t json_size,
        const uint8_t* const bin_data,
        const size_t bin_size
    );

    JsonParseResult parse_json_bin(
        std::vector<SceneIntermediate>& scenes,
        const BinDataView& json_src,
        const BinDataView& bin_src
    );

}  // namespace dal::parser
