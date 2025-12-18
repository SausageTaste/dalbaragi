#pragma once

#include <array>
#include <cstdint>
#include <string>


namespace dal {

    class BundleHeader {

    public:
        void init();

        bool is_magic_valid() const noexcept;

        std::string created_datetime() const noexcept {
            return std::string{ created_datetime_.begin(),
                                created_datetime_.end() };
        }
        uint64_t version() const noexcept { return version_; }

        uint64_t items_offset() const noexcept { return items_offset_; }
        uint64_t items_size() const noexcept { return items_size_; }
        uint64_t items_size_z() const noexcept { return items_size_z_; }
        uint64_t items_count() const noexcept { return items_count_; }

        uint64_t data_offset() const noexcept { return data_offset_; }
        uint64_t data_size() const noexcept { return data_size_; }
        uint64_t data_size_z() const noexcept { return data_size_z_; }

        void set_items_info(
            uint64_t offset, uint64_t size, uint64_t size_z, uint64_t count
        ) {
            items_offset_ = offset;
            items_size_ = size;
            items_size_z_ = size_z;
            items_count_ = count;
        }
        void set_data_info(uint64_t offset, uint64_t size, uint64_t size_z) {
            data_offset_ = offset;
            data_size_ = size;
            data_size_z_ = size_z;
        }

    private:
        std::array<char, 6> magic_;
        std::array<char, 32 - 6> created_datetime_;
        uint64_t version_;

        uint64_t items_offset_;
        uint64_t items_size_;
        uint64_t items_size_z_;  // Compressed size
        uint64_t items_count_;

        uint64_t data_offset_;
        uint64_t data_size_;
        uint64_t data_size_z_;  // Compressed size
    };


    struct BundleItemEntry {
        std::string name_;
        uint64_t offset_;
        uint64_t size_;
    };

}  // namespace dal
