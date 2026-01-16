#pragma once

#include <vector>

#include <sung/basic/byte_type.hpp>


namespace dal {

    using byte8 = sung::byte8;
    using binvec_t = std::vector<byte8>;


    class BinDataView {

    public:
        BinDataView() = default;

        BinDataView(const byte8* data, const size_t size)
            : data_(data), size_(size) {}

        BinDataView(const binvec_t& data)
            : data_(data.data()), size_(data.size()) {}

        const byte8* data() const { return this->data_; }
        size_t size() const { return this->size_; }

    private:
        const byte8* data_ = nullptr;
        size_t size_ = 0;
    };

}  // namespace dal
