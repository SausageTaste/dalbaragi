#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "dal/common/decompress.hpp"


namespace dal {

    CompressResultData compress_zip(
        uint8_t* const dst,
        const size_t dst_size,
        const uint8_t* const src,
        const size_t src_size
    );
    std::optional<binvec_t> compress_zip(const uint8_t* src, size_t src_size);
    std::optional<binvec_t> compress_zip(const BinDataView& src);


    constexpr uint32_t DEFAULT_BRO_QUALITY = 6;

    std::optional<binvec_t> compress_bro(
        const uint8_t* src,
        size_t src_size,
        uint32_t quality = DEFAULT_BRO_QUALITY
    );
    std::optional<binvec_t> compress_bro(
        const BinDataView& src, uint32_t quality = DEFAULT_BRO_QUALITY
    );


    std::optional<std::vector<uint8_t>> compress_with_header(
        const BinDataView& src
    );


    std::string encode_base64(const uint8_t* src, size_t src_size);
    std::string encode_base64(const BinDataView& src);

}  // namespace dal
