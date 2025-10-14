#include "daltools/dsd/struct.hpp"


namespace {

    constexpr uint32_t DSD_MAGIC = ('F') | ('D' << 8) | ('S' << 16) |
                                   ('D' << 24);

}  // namespace


namespace dal::dsd {

    bool Header::is_magic_valid() const noexcept { return magic_ == DSD_MAGIC; }

    void Header::set_magic() { magic_ = DSD_MAGIC; }

}  // namespace dal::dsd
