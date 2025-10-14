#pragma once

#include <vector>

#include <glm/vec4.hpp>
#include <sung/basic/mamath.hpp>

#include "daltools/img/img.hpp"


namespace dal {

    class IImage2D : public IImage {

    public:
        virtual uint32_t width() const = 0;
        virtual uint32_t height() const = 0;

        /**
         * Channels per pixel.
         *  - 1 = R
         *  - 2 = RG
         *  - 3 = RGB
         *  - 4 = RGBA
         */
        virtual uint32_t channels() const = 0;

        /**
         * sizeof(T)
         *  - 1 = `uint8_t`
         *  - 4 = `float`
         */
        virtual uint32_t value_type_size() const = 0;
    };


    template <typename T>
    class TImage2D : public IImage2D {

    public:
        // `x` is in range [0, width), `y` is in range [0, height)
        virtual T* texel_ptr(int x, int y) = 0;

        // `x` is in range [0, width), `y` is in range [0, height)
        virtual const T* texel_ptr(int x, int y) const = 0;
    };


    template <typename T>
    class TDataImage2D : public dal::TImage2D<T> {

    public:
        bool init(
            const T* data, uint32_t width, uint32_t height, uint32_t channels
        ) {
            const auto data_size = width * height * channels;
            if (nullptr != data) {
                data_.assign(data, data + data_size);
            } else {
                data_.resize(data_size);
            }

            this->width_ = width;
            this->height_ = height;
            this->channels_ = channels;
            return true;
        }

        void destroy() override { data_.clear(); }

        bool is_ready() const override {
            if (data_.empty())
                return false;
            if (0 == width_ || 0 == height_ || 0 == channels_)
                return false;
            return true;
        }

        uint32_t width() const override { return width_; }
        uint32_t height() const override { return height_; }
        uint32_t channels() const override { return channels_; }
        uint32_t value_type_size() const override { return sizeof(T); }

        T* texel_ptr(int x, int y) override {
            x = sung::clamp<int>(x, 0, width_ - 1);
            y = sung::clamp<int>(y, 0, height_ - 1);
            const auto index = (x + width_ * y) * channels_;
            return data_.data() + index;
        }

        const T* texel_ptr(int x, int y) const override {
            x = sung::clamp<int>(x, 0, width_ - 1);
            y = sung::clamp<int>(y, 0, height_ - 1);
            const auto index = (x + width_ * y) * channels_;
            return data_.data() + index;
        }

        const uint8_t* data() const {
            return reinterpret_cast<const uint8_t*>(data_.data());
        }

        uint32_t data_size() const {
            return static_cast<uint32_t>(data_.size() * sizeof(T));
        }

    private:
        std::vector<T> data_;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        uint32_t channels_ = 0;
    };

}  // namespace dal
