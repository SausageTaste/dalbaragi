#pragma once

#include "daltools/img/img2d.hpp"


namespace dal {

    std::unique_ptr<IImage2D> parse_img_stb(
        const uint8_t* data, size_t data_size, bool force_rgba
    );

}
