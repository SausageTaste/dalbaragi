#include "daltools/img/backend/ktx.hpp"


namespace dal {

    KtxImage::KtxImage(const uint8_t* data, size_t size) {
        const auto res = ktxTexture_CreateFromMemory(data, size, 0, &texture_);

        if (KTX_SUCCESS != res) {
            this->destroy();
            return;
        }
    }

    void KtxImage::destroy() {
        if (nullptr != texture_) {
            ktxTexture_Destroy(texture_);
            texture_ = nullptr;
        }
    }

    bool KtxImage::is_ready() const { return nullptr != texture_; }

    uint32_t KtxImage::base_width() const {
        if (nullptr == texture_)
            return 0;
        return texture_->baseWidth;
    }

    uint32_t KtxImage::base_height() const {
        if (nullptr == texture_)
            return 0;
        return texture_->baseHeight;
    }

    uint32_t KtxImage::esize() { return ktxTexture_GetElementSize(texture_); }

    uint32_t KtxImage::num_cpnts() {
        if (auto ktx2 = this->ktx2())
            return ktxTexture2_GetNumComponents(ktx2);
        else
            return 0;
    }

    bool KtxImage::need_transcoding() {
        if (nullptr == texture_)
            return false;
        return ktxTexture_NeedsTranscoding(texture_);
    }

    bool KtxImage::transcode(
        ktx_transcode_fmt_e format, ktx_transcode_flags flags
    ) {
        if (auto ktx2 = this->ktx2()) {
            const auto res = ktxTexture2_TranscodeBasis(ktx2, format, flags);
            return res == KTX_SUCCESS;
        }

        return false;
    }

    std::optional<glm::tvec4<uint8_t>> KtxImage::get_base_pixel(
        uint32_t x, uint32_t y
    ) const {
        auto data = ktxTexture_GetData(texture_);
        if (nullptr == data)
            return std::nullopt;

        const auto w = texture_->baseWidth;
        const auto h = texture_->baseHeight;
        if (x >= w || y >= h)
            return std::nullopt;

        constexpr auto texel_size = sizeof(glm::tvec4<uint8_t>);
        const auto supposed_data_size = texel_size * w * h;
        const auto data_size = ktxTexture_GetImageSize(texture_, 0);
        if (data_size != supposed_data_size)
            return std::nullopt;

        ktx_size_t data_offset = 0;
        ktxTexture_GetImageOffset(texture_, 0, 0, 0, &data_offset);

        const auto row_pitch = ktxTexture_GetRowPitch(texture_, 0);
        const auto row_ptr = data + data_offset + (y * row_pitch);
        const auto texel_ptr = row_ptr + (x * texel_size);
        return *reinterpret_cast<const glm::tvec4<uint8_t>*>(texel_ptr);
    }

    ktxTexture& KtxImage::ktx() { return *texture_; }

    ktxTexture1* KtxImage::ktx1() {
        if (texture_->classId == ktxTexture1_c)
            return reinterpret_cast<ktxTexture1*>(texture_);
        else
            return nullptr;
    }

    ktxTexture2* KtxImage::ktx2() {
        if (texture_->classId == ktxTexture2_c)
            return reinterpret_cast<ktxTexture2*>(texture_);
        else
            return nullptr;
    }

}  // namespace dal
