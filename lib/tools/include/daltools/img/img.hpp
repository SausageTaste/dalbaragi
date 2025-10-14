#pragma once

#include <memory>
#include <string>


namespace dal {

    class IImage {

    public:
        virtual ~IImage() = default;

        virtual void destroy() = 0;
        virtual bool is_ready() const = 0;

        template <typename T>
        T* as() {
            if (this->is_ready())
                return dynamic_cast<T*>(this);
            else
                return nullptr;
        }

        template <typename T>
        const T* as() const {
            if (this->is_ready())
                return dynamic_cast<const T*>(this);
            else
                return nullptr;
        }
    };


    struct ImageParseInfo {
        std::string file_path_ = "";
        const uint8_t* data_ = nullptr;
        size_t size_ = 0;
        bool force_rgba_ = false;
    };

    std::unique_ptr<IImage> parse_img(const ImageParseInfo& info);

}  // namespace dal
