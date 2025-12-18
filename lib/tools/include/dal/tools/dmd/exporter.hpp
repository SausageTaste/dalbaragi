#pragma once

#include <optional>
#include <vector>

#include "dal/parser/common/compress.hpp"
#include "dal/parser/scene/struct.hpp"


namespace dal {

    enum class ModelExportResult {
        success,
        compression_failure,
        unknown_error,
    };

    ModelExportResult build_binary_model(
        std::vector<uint8_t>& output,
        const Model& input,
        CompressMethod comp_method
    );

    std::optional<std::vector<uint8_t>> build_binary_model(
        const Model& input, CompressMethod comp_method
    );

}  // namespace dal
