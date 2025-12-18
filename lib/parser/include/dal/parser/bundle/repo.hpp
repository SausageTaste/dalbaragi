#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>


namespace dal {

    class BundleRepository {

    public:
        BundleRepository();
        ~BundleRepository();

        bool notify(const std::string& name, const std::vector<uint8_t>& data);

        std::pair<const uint8_t*, size_t> get_file_data(
            const std::string& bundle_name, const std::string& file_name
        ) const;

    private:
        struct Record;
        std::unordered_map<std::string, Record> records_;
    };

}  // namespace dal
