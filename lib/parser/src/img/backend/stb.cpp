#include "daltools/img/backend/stb.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


namespace dal {

    std::unique_ptr<IImage2D> parse_img_stb(
        const uint8_t* data, size_t data_size, bool force_rgba
    ) {
        int width, height, channels;
        const auto req_comp = force_rgba ? STBI_rgb_alpha : STBI_default;

        if (stbi_is_hdr_from_memory(data, static_cast<int>(data_size))) {
            const auto pixel_data = stbi_loadf_from_memory(
                data,
                static_cast<int>(data_size),
                &width,
                &height,
                &channels,
                req_comp
            );
            if (nullptr == pixel_data) {
                return nullptr;
            }

            auto image = std::make_unique<TDataImage2D<float>>();
            image->init(pixel_data, width, height, force_rgba ? 4 : channels);
            stbi_image_free(pixel_data);
            return image;
        } else {
            const auto pixel_data = stbi_load_from_memory(
                data,
                static_cast<int>(data_size),
                &width,
                &height,
                &channels,
                req_comp
            );
            if (nullptr == pixel_data) {
                return nullptr;
            }

            auto image = std::make_unique<TDataImage2D<uint8_t>>();
            image->init(pixel_data, width, height, force_rgba ? 4 : channels);
            stbi_image_free(pixel_data);
            return image;
        }
    }

}  // namespace dal
