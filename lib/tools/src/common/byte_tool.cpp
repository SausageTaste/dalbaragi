#include "daltools/common/byte_tool.h"


namespace dal::parser {

    bool is_big_endian() {
        constexpr short int number = 0x1;
        const char* const numPtr = reinterpret_cast<const char*>(&number);
        return numPtr[0] != 1;
    }

    bool make_bool8(const uint8_t* begin) {
        return (*begin) != static_cast<uint8_t>(0);
    }

    int32_t make_int16(const uint8_t* begin) {
        static_assert(1 == sizeof(uint8_t), "Size of uint8 is not 1 byte. WTF???");
        static_assert(4 == sizeof(float), "Size of float is not 4 bytes.");

        uint8_t buf[4];

        if ( is_big_endian() ) {
            buf[0] = 0;
            buf[1] = 0;
            buf[2] = begin[1];
            buf[3] = begin[0];
        }
        else {
            buf[0] = begin[0];
            buf[1] = begin[1];
            buf[2] = 0;
            buf[3] = 0;
        }

        int32_t res;
        memcpy(&res, buf, 4);
        return res;
    }

    int32_t make_int32(const uint8_t* begin) {
        return assemble_4_bytes<int32_t>(begin);
    }

    int64_t make_int64(const uint8_t* begin) {
        return assemble_8_bytes<int64_t>(begin);
    }

    float make_float32(const uint8_t* begin) {
        return assemble_4_bytes<float>(begin);
    }

}


namespace dal::parser {

    uint8_t to_bool8(const bool v) {
        return v ? 1 : 0;
    }

    void to_int16(const int32_t v, uint8_t* const buffer) {
        const auto src_loc = reinterpret_cast<const uint8_t*>(&v);

        if (is_big_endian()) {
            buffer[0] = src_loc[1];
            buffer[1] = src_loc[0];
        }
        else {
            std::memcpy(buffer, src_loc, 2);
        }
    }

    void to_int32(const int32_t v, uint8_t* const buffer) {
        const auto src_loc = reinterpret_cast<const uint8_t*>(&v);

        if (is_big_endian()) {
            buffer[0] = src_loc[3];
            buffer[1] = src_loc[2];
            buffer[2] = src_loc[1];
            buffer[3] = src_loc[0];
        }
        else {
            std::memcpy(buffer, src_loc, 4);
        }
    }

    void to_float32(const float v, uint8_t* const buffer) {
        const auto src_loc = reinterpret_cast<const uint8_t*>(&v);

        if (is_big_endian()) {
            buffer[0] = src_loc[3];
            buffer[1] = src_loc[2];
            buffer[2] = src_loc[1];
            buffer[3] = src_loc[0];
        }
        else {
            std::memcpy(buffer, src_loc, 4);
        }
    }

}


// BinaryDataArray
namespace dal::parser {

    BinaryDataArray& BinaryDataArray::operator+=(const BinaryDataArray& other) {
        this->m_vector.insert(this->m_vector.end(), other.m_vector.begin(), other.m_vector.end());
        return *this;
    }

    const uint8_t* BinaryDataArray::data() const {
        return this->m_vector.data();
    }

    size_t BinaryDataArray::size() const {
        return this->m_vector.size();
    }

    void BinaryDataArray::reserve(const size_t reserve_size) {
        this->m_vector.reserve(reserve_size);
    }

    void BinaryDataArray::push_back(const uint8_t v) {
        this->m_vector.push_back(v);
    }

    void BinaryDataArray::append_bool8(const bool v) {
        this->push_back(v ? 1 : 0);
    }

    void BinaryDataArray::append_int32(const int32_t v) {
        this->append_array(&v, 1);
    }

    void BinaryDataArray::append_int32_array(const int32_t* const arr, const size_t arr_size) {
        this->append_array(arr, arr_size);
    }

    void BinaryDataArray::append_int64(const int64_t v) {
        this->append_array(&v, 1);
    }

    void BinaryDataArray::append_float32(const float v) {
        this->append_array(&v, 1);
    }

    void BinaryDataArray::append_float32_array(const float* const arr, const size_t arr_size) {
        this->append_array(arr, arr_size);
    }

    void BinaryDataArray::append_char(const char c) {
        this->push_back(c);
    }

    void BinaryDataArray::append_null_terminated_str(const char* const str, const size_t str_size) {
        this->append_array(str, str_size);
        this->push_back(0);
    }

    void BinaryDataArray::append_str(const std::string& str) {
        this->append_null_terminated_str(str.data(), str.size());
    }

}


// BinaryArrayParser
namespace dal::parser {

    BinaryArrayParser::BinaryArrayParser(const uint8_t* const array, const size_t size)
        : m_array(array)
        , m_size(size)
        , pos_(0)
    {

    }

    BinaryArrayParser::BinaryArrayParser(const std::vector<uint8_t>& vector)
        : m_array(vector.data())
        , m_size(vector.size())
        , pos_(0)
    {

    }

    bool BinaryArrayParser::parse_bool8() {
        this->pos_ += 1;
        return this->m_array[this->pos_ - 1] != 0 ? true : false;
    }

    int32_t BinaryArrayParser::parse_int32() {
        return this->parse_one<int32_t>();
    }

    void BinaryArrayParser::parse_int32_array(int32_t* const arr, const size_t arr_size) {
        this->parse_array(arr, arr_size);
    }

    int64_t BinaryArrayParser::parse_int64() {
        return this->parse_one<int64_t>();
    }

    float BinaryArrayParser::parse_float32() {
        return this->parse_one<float>();
    }

    void BinaryArrayParser::parse_float32_array(float* const arr, const size_t arr_size) {
        this->parse_array(arr, arr_size);
    }

    char BinaryArrayParser::parse_char() {
        return this->parse_one<char>();
    }

    std::string BinaryArrayParser::parse_str() {
        const std::string output = reinterpret_cast<const char*>(this->m_array + this->pos_);
        this->pos_ += output.size() + 1;
        return output;
    }

}
