#include "daltools/common/compress.hpp"

#include <array>

#include <brotli/encode.h>
#include <libbase64.h>
#include <zlib.h>
#include <sung/basic/mamath.hpp>

#include "dal/auxiliary/byte_tool.hpp"


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
            dal::BinaryDataArray output;
            output.append_int64(src.size());
            output.append_array(buffer.data(), result.m_output_size);
            return output.release();
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

}  // namespace dal
