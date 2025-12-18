#pragma once

#include <filesystem>
#include <optional>


#if false
namespace std {

    template <>
    struct hash<std::filesystem::path> {
        size_t operator()(const std::filesystem::path& path) const {
            return std::hash<std::string>{}(path.u8string());
        }
    };

}  // namespace std
#endif


namespace dal {

    namespace fs = std::filesystem;
    using path = fs::path;


    path u8path(const char* s);
    path u8path(const char* s, size_t n);
    path u8path(const std::string& s);

    std::string tostr(const path& p);


    std::optional<path> find_parent_path_that_has(
        const path& path, const std::string& item_name_ext
    );
    std::optional<path> find_parent_path_that_has(
        const std::string& item_name_ext
    );

}  // namespace dal
