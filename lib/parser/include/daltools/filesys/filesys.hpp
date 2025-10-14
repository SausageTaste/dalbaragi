#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <sung/basic/bytes.hpp>


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
    using byte8 = sung::byte8;
    using path = fs::path;


    std::optional<fs::path> find_parent_path_that_has(
        const fs::path& path, const std::string& item_name_ext
    );
    std::optional<fs::path> find_parent_path_that_has(
        const std::string& item_name_ext
    );


    class IFileSubsys {

    public:
        virtual ~IFileSubsys() = default;

        virtual bool is_file(const fs::path& path) = 0;

        virtual bool read_file(
            const fs::path& path, std::vector<uint8_t>& out
        ) = 0;

        virtual bool read_file(
            const fs::path& path, std::vector<std::byte>& out
        ) = 0;
    };


    class Filesystem {

    public:
        Filesystem();
        ~Filesystem();

        void add_subsys(std::unique_ptr<IFileSubsys> subsys);

        bool is_file(const fs::path& path);

        bool read_file(const fs::path& path, std::vector<byte8>& out);
        bool read_file(const fs::path& path, std::vector<std::byte>& out);
        std::optional<std::vector<byte8>> read_file(const fs::path& path);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };
    using HFilesys = std::shared_ptr<Filesystem>;


    std::unique_ptr<IFileSubsys> create_filesubsys_std(
        const std::string& prefix, const fs::path& root
    );

}  // namespace dal
