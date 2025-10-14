#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "daltools/common/bin_data.h"


namespace dal {

    enum class CompressMethod {
        none,
        zip,
        brotli,
    };


    enum class CompressResult {
        success,
        not_enough_buffer_size,
        insufficient_memory,
        corrupted_data,
        unknown_error,
    };

    struct CompressResultData {
        size_t m_output_size;
        CompressResult m_result;
    };


    CompressResultData compress_zip(
        uint8_t* const dst,
        const size_t dst_size,
        const uint8_t* const src,
        const size_t src_size
    );
    std::optional<binvec_t> compress_zip(const uint8_t* src, size_t src_size);
    std::optional<binvec_t> compress_zip(const BinDataView& src);


    CompressResultData decomp_zip(
        uint8_t* const dst,
        const size_t dst_size,
        const uint8_t* const src,
        const size_t src_size
    );
    std::optional<binvec_t> decomp_zip(const BinDataView& src, size_t hint);


    constexpr uint32_t DEFAULT_BRO_QUALITY = 6;

    std::optional<binvec_t> compress_bro(
        const uint8_t* src,
        size_t src_size,
        uint32_t quality = DEFAULT_BRO_QUALITY
    );
    std::optional<binvec_t> compress_bro(
        const BinDataView& src, uint32_t quality = DEFAULT_BRO_QUALITY
    );

    std::optional<binvec_t> decomp_bro(
        const uint8_t* src, size_t src_size, size_t hint
    );
    std::optional<binvec_t> decomp_bro(const BinDataView& src, size_t hint);


    std::optional<std::vector<uint8_t>> compress_with_header(
        const BinDataView& src
    );

    std::optional<std::vector<uint8_t>> decompress_with_header(
        const BinDataView& src
    );


    std::string encode_base64(const uint8_t* src, size_t src_size);
    std::string encode_base64(const BinDataView& src);

    std::optional<std::vector<uint8_t>> decode_base64(
        const char* const base64_data, const size_t data_size
    );

}  // namespace dal
