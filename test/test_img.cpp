#include <filesystem>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/fmt/fmt.h>

#include "dal/auxiliary/util.hpp"
#include "daltools/img/backend/ktx.hpp"
#include "daltools/img/backend/stb.hpp"


namespace {

    std::string format_bytes(size_t bytes) {
        if (bytes < 1024) {
            return fmt::format("{} B", bytes);
        } else if (bytes < 1024 * 1024) {
            return fmt::format("{:.1f} KiB", bytes / 1024.0);
        } else if (bytes < 1024 * 1024 * 1024) {
            return fmt::format("{:.1f} MiB", bytes / (1024.0 * 1024.0));
        } else {
            return fmt::format(
                "{:.1f} GiB", bytes / (1024.0 * 1024.0 * 1024.0)
            );
        }
    }


    TEST(DaltestKtx, SimpleTest) {
        const auto root_path = dal::find_git_repo_root(dal::fs::current_path());
        ASSERT_TRUE(root_path.has_value());
        const auto img_path = root_path.value() / "test" / "img";

        std::vector<std::unique_ptr<dal::IImage>> images;

        for (const auto& entry : dal::fs::directory_iterator(img_path)) {
            const auto img_data = dal::read_file(entry.path());
            dal::ImageParseInfo parse_info;
            parse_info.data_ = img_data.data();
            parse_info.size_ = img_data.size();
            parse_info.force_rgba_ = false;
            parse_info.file_path_ = entry.path().string();

            auto img = dal::parse_img(parse_info);
            ASSERT_TRUE(img->is_ready());

            if (auto ktx_img = img->as<dal::KtxImage>()) {
                if (auto ktx1 = ktx_img->ktx1()) {
                    fmt::print(
                        "KTX 1: dim={}x{}x{}, levels={}, compressed={}, "
                        "dataSize={}\n",
                        ktx1->baseWidth,
                        ktx1->baseHeight,
                        ktx1->baseDepth,
                        ktx1->numLevels,
                        ktx1->isCompressed,
                        ::format_bytes(ktx1->dataSize)
                    );
                } else if (auto ktx2 = ktx_img->ktx2()) {
                    auto& tex = ktx_img->ktx();
                    auto& te2 = *ktx2;

                    if (ktx_img->need_transcoding()) {
                        const auto res = ktx_img->transcode(KTX_TTF_RGBA32);
                        ASSERT_TRUE(res);
                    }

                    size_t pixel_count = 0;
                    size_t transp_count = 0;
                    if (ktx_img->esize() == 4) {
                        const auto width = ktx_img->base_width();
                        const auto height = ktx_img->base_height();

                        for (uint32_t y = 0; y < height; ++y) {
                            for (uint32_t x = 0; x < width; ++x) {
                                ++pixel_count;
                                if (auto p = ktx_img->get_base_pixel(x, y)) {
                                    if (p->a < 254) {
                                        ++transp_count;
                                    }
                                }
                            }
                        }
                    }

                    fmt::print(
                        "KTX 2: dim={}x{}x{}, levels={}, compressed={}, "
                        "dataSize={}, transp={}/{}\n",
                        tex.baseWidth,
                        tex.baseHeight,
                        tex.baseDepth,
                        tex.numLevels,
                        tex.isCompressed,
                        ::format_bytes(tex.dataSize),
                        transp_count,
                        pixel_count
                    );
                    continue;
                }
            } else if (auto img2d = img->as<dal::IImage2D>()) {
                size_t pixel_count = 0;
                size_t transp_count = 0;

                if (auto img_u8 = img->as<dal::TImage2D<uint8_t>>()) {
                    for (uint32_t y = 0; y < img_u8->height(); ++y) {
                        for (uint32_t x = 0; x < img_u8->width(); ++x) {
                            ++pixel_count;
                            if (auto p = img_u8->texel_ptr(x, y)) {
                                if (p[3] < 254) {
                                    ++transp_count;
                                }
                            }
                        }
                    }
                } else if (auto img_f32 = img->as<dal::TImage2D<float>>()) {
                    for (uint32_t y = 0; y < img_f32->height(); ++y) {
                        for (uint32_t x = 0; x < img_f32->width(); ++x) {
                            ++pixel_count;
                            if (auto p = img_f32->texel_ptr(x, y)) {
                                if (p[3] < 0.99f) {
                                    ++transp_count;
                                }
                            }
                        }
                    }
                }

                fmt::print(
                    "IImage2D: dim={}x{}, ch={}, typeSize={}, transp={}/{}\n",
                    img2d->width(),
                    img2d->height(),
                    img2d->channels(),
                    img2d->value_type_size(),
                    transp_count,
                    pixel_count
                );
            }
        }

        return;
    }

}  // namespace


int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
