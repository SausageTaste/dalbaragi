#include "daltools/common/compression.h"

#include <array>

#include <brotli/decode.h>
#include <brotli/encode.h>
#include <libbase64.h>
#include <zlib.h>

#include <sung/basic/mamath.hpp>

#include "daltools/common/byte_tool.h"


namespace {

    constexpr int BROTLI_BUFFER_SIZE = 1024 * 1024 * 1;

}


namespace dal {

    CompressResultData compress_zip(
        uint8_t* const dst,
        const size_t dst_size,
        const uint8_t* const src,
        const size_t src_size
    ) {
        CompressResultData output;

        const auto src_len = static_cast<uLongf>(src_size);
        auto dst_len = static_cast<uLongf>(dst_size);

        if (Z_OK == compress(dst, &dst_len, src, src_len)) {
            output.m_result = CompressResult::success;
            output.m_output_size = dst_len;
        } else {
            output.m_result = CompressResult::unknown_error;
        }

        return output;
    }

    std::optional<binvec_t> compress_zip(const uint8_t* src, size_t src_size) {
        binvec_t output;
        output.resize(src_size * 2);

        const auto res = compress_zip(
            output.data(), output.size(), src, src_size
        );
        if (res.m_result != CompressResult::success) {
            return std::nullopt;
        }

        output.resize(res.m_output_size);
        return output;
    }

    std::optional<binvec_t> compress_zip(const BinDataView& src) {
        return compress_zip(src.data(), src.size());
    }

    CompressResultData decomp_zip(
        uint8_t* const dst,
        const size_t dst_size,
        const uint8_t* const src,
        const size_t src_size
    ) {
        static_assert(sizeof(Bytef) == sizeof(uint8_t));

        CompressResultData output;
        const auto src_len = static_cast<uLongf>(src_size);
        uLongf decom_buffer_size = static_cast<uLongf>(dst_size);

        const auto res = uncompress(dst, &decom_buffer_size, src, src_len);
        switch (res) {
            case Z_OK:
                output.m_result = CompressResult::success;
                output.m_output_size = decom_buffer_size;
                break;
            case Z_BUF_ERROR:
                output.m_result = CompressResult::not_enough_buffer_size;
                break;
            case Z_MEM_ERROR:
                output.m_result = CompressResult::insufficient_memory;
                break;
            case Z_DATA_ERROR:
                output.m_result = CompressResult::corrupted_data;
                break;
            default:
                output.m_result = CompressResult::unknown_error;
                break;
        }

        return output;
    }

    std::optional<binvec_t> decomp_zip(const BinDataView& src, size_t hint) {
        binvec_t output;
        if (0 != hint)
            output.resize(hint);
        else
            output.resize(src.size() * 4);

        const auto res = decomp_zip(
            output.data(), output.size(), src.data(), src.size()
        );
        if (res.m_result != CompressResult::success) {
            return std::nullopt;
        }

        output.resize(res.m_output_size);
        return output;
    }


    std::optional<binvec_t> compress_bro(
        const uint8_t* const src, const size_t src_size, uint32_t q
    ) {
        auto instance = BrotliEncoderCreateInstance(nullptr, nullptr, nullptr);
        BrotliEncoderSetParameter(
            instance,
            BROTLI_PARAM_QUALITY,
            sung::clamp<uint32_t>(q, BROTLI_MIN_QUALITY, BROTLI_MAX_QUALITY)
        );

        std::vector<uint8_t> buffer(BROTLI_BUFFER_SIZE);
        binvec_t result;

        auto available_in = src_size;
        auto available_out = buffer.size();
        auto next_in = src;
        auto next_out = buffer.data();

        do {
            BrotliEncoderCompressStream(
                instance,
                BROTLI_OPERATION_FINISH,
                &available_in,
                &next_in,
                &available_out,
                &next_out,
                nullptr
            );
            result.insert(
                result.end(), buffer.begin(), buffer.end() - available_out
            );
            available_out = buffer.size();
            next_out = buffer.data();
        } while (!(available_in == 0 && BrotliEncoderIsFinished(instance)));

        BrotliEncoderDestroyInstance(instance);
        return result;
    }

    std::optional<binvec_t> compress_bro(
        const BinDataView& src, uint32_t quality
    ) {
        return compress_bro(src.data(), src.size(), quality);
    }

    std::optional<binvec_t> decomp_bro(
        const uint8_t* src, size_t src_size, size_t hint
    ) {
        auto instance = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
        std::vector<uint8_t> buffer(BROTLI_BUFFER_SIZE);
        binvec_t result;
        result.reserve(hint);

        auto available_in = src_size;
        auto available_out = buffer.size();
        auto next_in = src;
        auto next_out = buffer.data();
        BrotliDecoderResult oneshot_result;

        do {
            oneshot_result = BrotliDecoderDecompressStream(
                instance,
                &available_in,
                &next_in,
                &available_out,
                &next_out,
                nullptr
            );
            result.insert(
                result.end(), buffer.begin(), buffer.end() - available_out
            );
            available_out = buffer.size();
            next_out = buffer.data();
        } while (!(
            available_in == 0 && oneshot_result == BROTLI_DECODER_RESULT_SUCCESS
        ));

        BrotliDecoderDestroyInstance(instance);
        return result;
    }

    std::optional<binvec_t> decomp_bro(const BinDataView& src, size_t hint) {
        return decomp_bro(src.data(), src.size(), hint);
    }


    std::optional<std::vector<uint8_t>> compress_with_header(
        const BinDataView& src
    ) {
        std::vector<uint8_t> buffer(src.size() * 2);
        const auto result = compress_zip(
            buffer.data(), buffer.size(), src.data(), src.size()
        );

        if (result.m_result != CompressResult::success) {
            return std::nullopt;
        } else {
            dal::parser::BinaryDataArray output;
            output.append_int64(src.size());
            output.append_array(buffer.data(), result.m_output_size);
            return output.release();
        }
    }

    std::optional<std::vector<uint8_t>> decompress_with_header(
        const BinDataView& src
    ) {
        constexpr size_t HEADER_SIZE = sizeof(int64_t);

        dal::parser::BinaryArrayParser parser(src.data(), src.size());
        const auto raw_data_size = parser.parse_int64();
        std::vector<uint8_t> buffer(raw_data_size);

        const auto result = decomp_zip(
            buffer.data(),
            buffer.size(),
            src.data() + HEADER_SIZE,
            src.size() - HEADER_SIZE
        );
        if (result.m_result != CompressResult::success) {
            return std::nullopt;
        } else if (result.m_output_size != raw_data_size) {
            return std::nullopt;
        } else {
            return buffer;
        }
    }


    std::string encode_base64(const uint8_t* src, size_t src_size) {
        std::string output;
        output.resize(src_size * 2);
        size_t output_size = output.size();

        base64_encode(
            reinterpret_cast<const char*>(src),
            src_size,
            reinterpret_cast<char*>(output.data()),
            &output_size,
            0
        );

        output.resize(output_size);
        return output;
    }

    std::string encode_base64(const BinDataView& src) {
        return encode_base64(src.data(), src.size());
    }

    std::optional<std::vector<uint8_t>> decode_base64(
        const char* const base64_data, const size_t data_size
    ) {
        std::vector<uint8_t> output(data_size);
        size_t output_size = output.size();

        const auto result = base64_decode(
            base64_data,
            data_size,
            reinterpret_cast<char*>(output.data()),
            &output_size,
            0
        );

        if (1 != result)
            return std::nullopt;

        output.resize(output_size);
        return output;
    }

}  // namespace dal
