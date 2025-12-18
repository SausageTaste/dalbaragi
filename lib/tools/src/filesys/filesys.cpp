#include "dal/filesys/filesys.hpp"

#include <mutex>

#include <spdlog/fmt/fmt.h>
#include <fstream>
#include <sung/basic/bytes.hpp>

#include "dal/bundle/bundle.hpp"
#include "dal/bundle/repo.hpp"
#include "dal/common/decompress.hpp"


namespace {

    namespace fs = std::filesystem;

}  // namespace


namespace {

    class FileSubsysStd : public dal::IFileSubsys {

    public:
        FileSubsysStd(const std::string& prefix, const fs::path& root)
            : prefix_{ prefix }, root_{ root } {}

        bool is_file(const fs::path& i_path) override {
            const auto raw_path = this->make_raw_path(i_path);
            if (!raw_path.has_value())
                return false;

            if (fs::is_regular_file(raw_path.value()))
                return true;

            return false;
        }

        bool read_file(
            const fs::path& i_path, std::vector<uint8_t>& out
        ) override {
            const auto raw_path = this->make_raw_path(i_path);
            if (!raw_path.has_value())
                return false;

            std::ifstream file(*raw_path, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                const auto content_size = file.tellg();
                out.resize(content_size);
                file.seekg(0, std::ios::beg);
                file.read(reinterpret_cast<char*>(out.data()), out.size());
                return true;
            }

            return false;
        }

        bool read_file(
            const fs::path& i_path, std::vector<std::byte>& out
        ) override {
            const auto raw_path = this->make_raw_path(i_path);
            if (!raw_path.has_value())
                return false;

            std::ifstream file(*raw_path, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                const auto content_size = file.tellg();
                out.resize(content_size);
                file.seekg(0, std::ios::beg);
                file.read(reinterpret_cast<char*>(out.data()), out.size());
                return true;
            }

            return false;
        }

    private:
        std::optional<fs::path> make_raw_path(const fs::path& i_path) const {
            const auto prefix_str = dal::tostr(prefix_);
            const auto interf_path_str = dal::tostr(i_path);
            if (interf_path_str.find(prefix_str) != 0) {
                return std::nullopt;
            } else {
                const auto suffix = interf_path_str.substr(prefix_str.size());
                const auto out = dal::u8path(dal::tostr(root_) + '/' + suffix);
                return fs::absolute(out);
            }
        }

        fs::path make_i_path(const fs::path& raw_path) const {
            const auto rel_path = fs::relative(raw_path, root_);
            return prefix_ / rel_path;
        }

        fs::path root_;
        fs::path prefix_;
    };

}  // namespace


namespace dal {

    std::unique_ptr<IFileSubsys> create_filesubsys_std(
        const std::string& prefix, const fs::path& root
    ) {
        return std::make_unique<FileSubsysStd>(prefix, root);
    }

}  // namespace dal


namespace dal {

    class Filesystem::Impl {

    public:
        std::mutex mut_;
        BundleRepository bundles_;
        std::vector<std::unique_ptr<IFileSubsys>> subsys_;
    };


    Filesystem::Filesystem() : pimpl_(std::make_unique<Impl>()) {}
    Filesystem::~Filesystem() {}

    void Filesystem::add_subsys(std::unique_ptr<IFileSubsys> subsys) {
        std::lock_guard<std::mutex> lock{ this->pimpl_->mut_ };
        pimpl_->subsys_.push_back(std::move(subsys));
    }

    bool Filesystem::is_file(const fs::path& path) {
        std::lock_guard<std::mutex> lock{ this->pimpl_->mut_ };

        for (const auto& subsys : this->pimpl_->subsys_) {
            if (subsys->is_file(path)) {
                return true;
            }
        }

        // Check if the file is in a bundle
        const auto parent_path = path.parent_path();
        for (const auto& subsys : this->pimpl_->subsys_) {
            if (!subsys->is_file(parent_path))
                continue;

            auto bundle_file_data = pimpl_->bundles_.get_file_data(
                dal::tostr(parent_path), dal::tostr(path.filename())
            );
            if (nullptr != bundle_file_data.first)
                return true;

            std::vector<uint8_t> file_content;
            if (!subsys->read_file(parent_path, file_content))
                continue;
            if (!pimpl_->bundles_.notify(dal::tostr(parent_path), file_content))
                continue;

            bundle_file_data = pimpl_->bundles_.get_file_data(
                dal::tostr(parent_path), dal::tostr(path.filename())
            );
            if (nullptr != bundle_file_data.first)
                return true;
        }

        return false;
    }

    bool Filesystem::read_file(
        const fs::path& path, std::vector<uint8_t>& out
    ) {
        std::lock_guard<std::mutex> lock{ this->pimpl_->mut_ };

        for (const auto& subsys : this->pimpl_->subsys_) {
            if (subsys->read_file(path, out)) {
                return true;
            }
        }

        // Check if the file is in a bundle
        const auto parent_path = path.parent_path();
        for (const auto& subsys : this->pimpl_->subsys_) {
            if (!subsys->is_file(parent_path))
                continue;

            auto bundle_file_data = pimpl_->bundles_.get_file_data(
                dal::tostr(parent_path), dal::tostr(path.filename())
            );
            if (nullptr != bundle_file_data.first) {
                out.assign(
                    bundle_file_data.first,
                    bundle_file_data.first + bundle_file_data.second
                );
                return true;
            }

            std::vector<uint8_t> file_content;
            if (!subsys->read_file(parent_path, file_content))
                continue;
            if (!pimpl_->bundles_.notify(dal::tostr(parent_path), file_content))
                continue;

            bundle_file_data = pimpl_->bundles_.get_file_data(
                dal::tostr(parent_path), dal::tostr(path.filename())
            );
            if (nullptr != bundle_file_data.first) {
                out.assign(
                    bundle_file_data.first,
                    bundle_file_data.first + bundle_file_data.second
                );
                return true;
            }
        }

        return false;
    }

    bool Filesystem::read_file(
        const fs::path& path, std::vector<std::byte>& out
    ) {
        std::lock_guard<std::mutex> lock{ this->pimpl_->mut_ };

        for (const auto& subsys : this->pimpl_->subsys_) {
            if (subsys->read_file(path, out)) {
                return true;
            }
        }

        // Check if the file is in a bundle
        const auto parent_path = path.parent_path();
        for (const auto& subsys : this->pimpl_->subsys_) {
            if (!subsys->is_file(parent_path))
                continue;

            auto [data_ptr, data_size] = pimpl_->bundles_.get_file_data(
                dal::tostr(parent_path), dal::tostr(path.filename())
            );
            if (nullptr != data_ptr) {
                auto p_data = reinterpret_cast<const std::byte*>(data_ptr);
                out.assign(p_data, p_data + data_size);
                return true;
            }

            std::vector<uint8_t> file_content;
            if (!subsys->read_file(parent_path, file_content))
                continue;
            if (!pimpl_->bundles_.notify(dal::tostr(parent_path), file_content))
                continue;

            std::tie(data_ptr, data_size) = pimpl_->bundles_.get_file_data(
                dal::tostr(parent_path), dal::tostr(path.filename())
            );
            if (nullptr != data_ptr) {
                auto p_data = reinterpret_cast<const std::byte*>(data_ptr);
                out.assign(p_data, p_data + data_size);
                return true;
            }
        }

        return false;
    }

    std::optional<std::vector<uint8_t>> Filesystem::read_file(
        const fs::path& path
    ) {
        std::vector<uint8_t> out;
        if (this->read_file(path, out))
            return out;
        else
            return std::nullopt;
    }

}  // namespace dal
