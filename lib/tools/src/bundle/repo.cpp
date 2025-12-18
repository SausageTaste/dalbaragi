#include "dal/bundle/repo.hpp"

#include <sung/basic/byte_arr.hpp>

#include "dal/bundle/bundle.hpp"
#include "dal/common/decompress.hpp"


namespace dal {

    struct BundleRepository::Record {
        bool try_load_data(const std::vector<uint8_t>& data) {
            if (data.size() < sizeof(BundleHeader))
                return false;
            auto& header = *reinterpret_cast<const BundleHeader*>(data.data());
            if (!header.is_magic_valid())
                return false;
            if (data.size() < header.data_offset() + header.data_size_z())
                return false;
            if (data_block_.size() == header.data_size())
                return true;

            const auto data_block = decomp_bro(
                data.data() + header.data_offset(),
                header.data_size_z(),
                header.data_size()
            );
            if (data_block.has_value()) {
                data_block_ = std::move(*data_block);
                return true;
            }

            return false;
        }

        std::vector<BundleItemEntry> items_;
        std::vector<uint8_t> data_block_;
    };


    BundleRepository::BundleRepository() = default;

    BundleRepository::~BundleRepository() = default;

    bool BundleRepository::notify(
        const std::string& name, const std::vector<uint8_t>& data
    ) {
        auto it = records_.find(name);
        if (records_.end() != it)
            return it->second.try_load_data(data);

        if (data.size() < sizeof(BundleHeader))
            return false;
        auto& header = *reinterpret_cast<const BundleHeader*>(data.data());
        if (!header.is_magic_valid())
            return false;

        if (data.size() < header.items_offset() + header.items_size_z())
            return false;

        const auto items_block = decomp_bro(
            data.data() + header.items_offset(),
            header.items_size_z(),
            header.items_size()
        );
        if (!items_block.has_value())
            return false;
        if (items_block->size() != header.items_size())
            return false;

        Record record;
        sung::BytesReader reader{ items_block->data(), items_block->size() };
        for (size_t i = 0; i < header.items_count(); ++i) {
            auto& entry = record.items_.emplace_back();
            entry.name_ = reader.read_nt_str();

            if (auto offset = reader.read_uint64())
                entry.offset_ = offset.value();
            else
                return false;

            if (auto size = reader.read_uint64())
                entry.size_ = size.value();
            else
                return false;
        }
        if (!reader.is_eof())
            return false;

        record.try_load_data(data);

        records_.emplace(name, std::move(record));
        return true;
    }

    std::pair<const uint8_t*, size_t> BundleRepository::get_file_data(
        const std::string& bundle_name, const std::string& file_name
    ) const {
        auto it = records_.find(bundle_name);
        if (records_.end() == it)
            return { nullptr, 0 };

        for (const auto& entry : it->second.items_) {
            if (entry.name_ == file_name)
                return { it->second.data_block_.data() + entry.offset_,
                         entry.size_ };
        }

        return { nullptr, 0 };
    }

}  // namespace dal
