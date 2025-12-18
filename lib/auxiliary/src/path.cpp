#include "dal/auxiliary/path.hpp"


namespace dal {

    path u8path(const char* s) { return dal::u8path(s, std::strlen(s)); }

    path u8path(const char* s, size_t n) {
        const auto ptr = reinterpret_cast<const char8_t*>(s);
        return fs::path(ptr, ptr + n);
    }

    path u8path(const std::string& s) {
        return dal::u8path(s.data(), s.size());
    }

    std::string tostr(const path& p) {
        const auto u8str = p.u8string();
        return std::string{ u8str.begin(), u8str.end() };
    }

    std::optional<fs::path> find_parent_path_that_has(
        const fs::path& path, const std::string& item_name_ext
    ) {
        auto current = path;
        while (true) {
            if (fs::exists(current / item_name_ext))
                return current;
            if (current == current.parent_path())
                return std::nullopt;
            current = current.parent_path();
        }
    }

    std::optional<fs::path> find_parent_path_that_has(
        const std::string& item_name_ext
    ) {
        return find_parent_path_that_has(fs::current_path(), item_name_ext);
    }

}  // namespace dal
