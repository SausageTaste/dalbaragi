#include "daltools/img/img.hpp"

#include "daltools/img/backend/ktx.hpp"
#include "daltools/img/backend/stb.hpp"


namespace dal {

    std::unique_ptr<IImage> parse_img(const ImageParseInfo& info) {
        {
            auto img = parse_img_stb(info.data_, info.size_, info.force_rgba_);
            if (img) {
                return img;
            }
        }

        {
            auto img = std::make_unique<KtxImage>(info.data_, info.size_);
            if (img->is_ready()) {
                return img;
            }
        }

        return nullptr;
    }

}  // namespace dal
