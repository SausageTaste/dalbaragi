#include "daltools/bundle/bundle.hpp"

#include <cstring>

#include <sung/basic/time.hpp>


// BundleHeader
namespace dal {

    void BundleHeader::init() {
        static_assert(sizeof(char) == 1);
        static_assert(offsetof(BundleHeader, magic_) == 0);
        static_assert(offsetof(BundleHeader, version_) == 32);

        std::memcpy(magic_.data(), "DALBUN", 6);

        const auto now = sung::get_time_iso_utc();
        created_datetime_.fill(0);
        std::memcpy(
            created_datetime_.data(),
            now.c_str(),
            std::min(now.size(), sizeof(created_datetime_))
        );

        version_ = 1;
        items_offset_ = 0;
        items_size_ = 0;
        items_count_ = 0;
        data_offset_ = 0;
        data_size_ = 0;
    }

    bool BundleHeader::is_magic_valid() const noexcept {
        return std::memcmp(magic_.data(), "DALBUN", 6) == 0;
    }

}  // namespace dal
