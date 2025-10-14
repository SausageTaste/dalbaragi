#pragma once

#include <cstdint>
#include <vector>


namespace dal {

    using binvec_t = std::vector<uint8_t>;


    class BinDataView {

    public:
        BinDataView() = default;

        BinDataView(const uint8_t* data, const size_t size)
            : data_(data), size_(size) {}

        BinDataView(const binvec_t& data)
            : data_(data.data()), size_(data.size()) {}

        const uint8_t* data() const { return this->data_; }
        size_t size() const { return this->size_; }

    private:
        const uint8_t* data_ = nullptr;
        size_t size_ = 0;
    };

}  // namespace dal::parser
