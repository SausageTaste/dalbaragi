#include "daltools/filesys/res_mgr.hpp"

#include <map>
#include <set>
#include <unordered_map>
#include <variant>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>

#include "daltools/dmd/parser.h"


namespace {

    namespace fs = dal::fs;


    dal::ResType deduce_res_type(const fs::path& path) {
        static const std::map<dal::ResType, std::set<std::string>> exts{
            { dal::ResType::img,
              { ".ktx", ".png", ".jpg", ".jpeg", ".bmp", ".tga" } },
            { dal::ResType::dmd, { ".dmd" } },
        };

        for (const auto& [type, ext_set] : exts) {
            if (ext_set.find(path.extension().u8string()) != ext_set.end())
                return type;
        }

        return dal::ResType::unknown;
    }


    /*
    class ResItem {

    public:
        ResItem(const fs::path& path) : path_(path) {}

        dal::ReqResult udpate(dal::Filesystem& filesys) {
            if (perma_res_.has_value())
                return perma_res_.value();

            if (res_data_.index() != 0)
                return this->set_per_res(dal::ReqResult::ready);

            if (raw_data_.empty()) {
                filesys.read_file(path_, raw_data_);
                if (raw_data_.empty())
                    return dal::ReqResult::cannot_read_file;
                else
                    return dal::ReqResult::loading;
            }

            if (res_data_.index() == 0) {
                switch (::deduce_res_type(path_)) {
                    case dal::ResType::img:
                        if (this->try_parse_img())
                            return this->set_per_res(dal::ReqResult::ready);
                    case dal::ResType::dmd:
                        if (this->try_parse_dmd())
                            return this->set_per_res(dal::ReqResult::ready);
                    default:
                        break;
                }
            }

            if (this->try_parse_img())
                return this->set_per_res(dal::ReqResult::ready);
            if (this->try_parse_dmd())
                return this->set_per_res(dal::ReqResult::ready);

            return this->set_per_res(dal::ReqResult::not_supported_file);
        }

        dal::ResType res_type() {
            switch (res_data_.index()) {
                case 1:
                    return dal::ResType::img;
                case 2:
                    return dal::ResType::dmd;
                default:
                    return dal::ResType::unknown;
            }
        }

        dal::IImage* get_img() {
            if (res_data_.index() == 1) {
                return std::get<1>(res_data_).get();
            } else {
                return nullptr;
            }
        }

        const dal::IImage* get_img() const {
            if (res_data_.index() == 1) {
                return std::get<1>(res_data_).get();
            } else {
                return nullptr;
            }
        }

        const dal::parser::Model* get_dmd() const {
            if (res_data_.index() == 2) {
                return &std::get<dal::parser::Model>(res_data_);
            } else {
                return nullptr;
            }
        }

    private:
        bool try_parse_img() {
            dal::ImageParseInfo pinfo;
            pinfo.file_path_ = path_.u8string();
            pinfo.data_ = reinterpret_cast<uint8_t*>(raw_data_.data());
            pinfo.size_ = raw_data_.size();
            pinfo.force_rgba_ = true;

            if (auto img = dal::parse_img(pinfo)) {
                res_data_ = std::move(img);
                return true;
            }

            return false;
        }

        bool try_parse_dmd() {
            res_data_ = dal::parser::Model{};
            const auto parse_result = dal::parser::parse_dmd(
                std::get<dal::parser::Model>(res_data_),
                reinterpret_cast<const uint8_t*>(raw_data_.data()),
                raw_data_.size()
            );

            if (parse_result != dal::parser::ModelParseResult::success) {
                res_data_ = std::monostate{};
                return false;
            }

            return true;
        }

        dal::ReqResult set_per_res(dal::ReqResult res) {
            perma_res_ = res;
            return res;
        }

        fs::path path_;
        std::vector<std::byte> raw_data_;
        std::variant<
            std::monostate,
            std::shared_ptr<dal::IImage>,
            dal::parser::Model>
            res_data_;
        std::optional<dal::ReqResult> perma_res_ = std::nullopt;
    };


    class ResAdaptor : public sung::IDataAdapter {

    public:
        ResAdaptor(
            const fs::path& path, std::shared_ptr<dal::Filesystem> filesys
        )
            : filesys_(filesys), path_(path) {}

        void load() override { result_ = this->load_result(); }

        void finalize() {}

        dal::IImage* get_img() {
            if (res_data_.index() == 1) {
                return std::get<1>(res_data_).get();
            } else {
                return nullptr;
            }
        }

        const dal::IImage* get_img() const {
            if (res_data_.index() == 1) {
                return std::get<1>(res_data_).get();
            } else {
                return nullptr;
            }
        }

        const dal::parser::Model* get_dmd() const {
            if (res_data_.index() == 2) {
                return &std::get<dal::parser::Model>(res_data_);
            } else {
                return nullptr;
            }
        }

        dal::ResType res_type() const {
            switch (res_data_.index()) {
                case 1:
                    return dal::ResType::img;
                case 2:
                    return dal::ResType::dmd;
                default:
                    return dal::ResType::unknown;
            }
        }

        std::optional<dal::ReqResult> result() const { return result_; }

    private:
        dal::ReqResult load_result() {
            if (!filesys_)
                return dal::ReqResult::cannot_read_file;

            filesys_->read_file(path_, raw_data_);
            if (raw_data_.empty())
                return dal::ReqResult::cannot_read_file;

            switch (::deduce_res_type(path_)) {
                case dal::ResType::img:
                    if (this->try_parse_img())
                        return dal::ReqResult::ready;
                case dal::ResType::dmd:
                    if (this->try_parse_dmd())
                        return dal::ReqResult::ready;
                default:
                    break;
            }

            if (this->try_parse_img())
                return dal::ReqResult::ready;
            if (this->try_parse_dmd())
                return dal::ReqResult::ready;

            return dal::ReqResult::not_supported_file;
        }

        bool try_parse_img() {
            dal::ImageParseInfo pinfo;
            pinfo.file_path_ = path_.u8string();
            pinfo.data_ = reinterpret_cast<uint8_t*>(raw_data_.data());
            pinfo.size_ = raw_data_.size();
            pinfo.force_rgba_ = true;

            if (auto img = dal::parse_img(pinfo)) {
                res_data_ = std::move(img);
                return true;
            }

            return false;
        }

        bool try_parse_dmd() {
            res_data_ = dal::parser::Model{};
            const auto parse_result = dal::parser::parse_dmd(
                std::get<dal::parser::Model>(res_data_),
                reinterpret_cast<const uint8_t*>(raw_data_.data()),
                raw_data_.size()
            );

            if (parse_result != dal::parser::ModelParseResult::success) {
                res_data_ = std::monostate{};
                return false;
            }

            return true;
        }

        std::shared_ptr<dal::Filesystem> filesys_;
        fs::path path_;
        std::vector<std::byte> raw_data_;
        std::variant<
            std::monostate,
            std::shared_ptr<dal::IImage>,
            dal::parser::Model>
            res_data_;
        std::optional<dal::ReqResult> result_;
    };


    class ResItemTask : public sung::ITask {

    public:
        ResItemTask(sung::DataReader& reader, sung::IDataCentral& datacen) {
            writer_ = datacen.get_writer_of(reader).value();
        }

        sung::TaskStatus tick() override {
            auto res_pair = writer_.get();
            if (!res_pair)
                return sung::TaskStatus::running;

            auto adaptor = dynamic_cast<::ResAdaptor*>(res_pair->second);
            if (!adaptor)
                return sung::TaskStatus::finished;

            adaptor->load();
            return sung::TaskStatus::finished;
        }

    private:
        sung::DataWriter writer_;
    };


    struct ResItem {
        void init(
            const fs::path& path,
            sung::ITaskScheduler& task_sche,
            sung::IDataCentral& datacen,
            std::shared_ptr<dal::Filesystem> filesys
        ) {
            auto adaptor = std::make_unique<::ResAdaptor>(path, filesys);
            reader_ = datacen.add(std::move(adaptor));
            writer_ = datacen.get_writer_of(reader_).value();

            auto task = std::make_shared<::ResItemTask>(reader_, datacen);
            task_sche.add_task(task);
            task_ = task;
        }

        dal::ResType res_type() {
            auto adaptor = this->get_reader_adaptor();
            if (!adaptor)
                return dal::ResType::unknown;

            return adaptor->get_img() ? dal::ResType::img : dal::ResType::dmd;
        }

        dal::ReqResult status() {
            auto res_pair = writer_.get();
            if (!res_pair)
                return dal::ReqResult::loading;

            auto adaptor = dynamic_cast<const ::ResAdaptor*>(res_pair->second);
            if (!adaptor)
                return dal::ReqResult::unknown_error;

            if (const auto res = adaptor->result())
                return res.value();

            return dal::ReqResult::loading;
        }

        const ::ResAdaptor* get_reader_adaptor() {
            auto res_pair = reader_.get();
            if (!res_pair)
                return nullptr;

            auto adaptor = dynamic_cast<const ::ResAdaptor*>(res_pair->second);
            if (!adaptor)
                return nullptr;

            return adaptor;
        }

        sung::DataReader reader_;
        sung::DataWriter writer_;
        std::shared_ptr<sung::ITask> task_;
    };


    class ResourceManager : public dal::IResourceManager {

    private:
        using ReadLock = dal::IResourceManager::ReadLock;
        using WriteLock = dal::IResourceManager::WriteLock;

        template <typename TResType, typename TLockType>
        using ResLckOpt = dal::IResourceManager::ResLckOpt<TResType, TLockType>;

    public:
        ResourceManager(
            std::shared_ptr<dal::Filesystem> filesys,
            sung::HTaskSche task_sche,
            sung::HDataCentral datacen
        )
            : filesys_(filesys), task_sche_(task_sche), datacen_(datacen) {}

        dal::ReqResult request(const fs::path& respath) override {
            auto& item = this->get_or_create(respath);
            const auto status = item.status();
            return status;
        }

        dal::ResType query_res_type(const fs::path& path) override {
            if (auto item = this->get(path))
                return item->res_type();

            return dal::ResType::unknown;
        }

        using ImmutableImage = ResLckOpt<const dal::IImage, ReadLock>;
        ImmutableImage get_img(const fs::path& path) override {
            auto item = this->get(path);
            if (!item)
                return std::nullopt;

            auto res_pair = item->reader_.get();
            if (!res_pair)
                return std::nullopt;

            auto adaptor = dynamic_cast<const ::ResAdaptor*>(res_pair->second);
            if (!adaptor)
                return std::nullopt;

            auto image = adaptor->get_img();
            if (!image)
                return std::nullopt;

            return std::make_pair(std::ref(*image), std::move(res_pair->first));
        }

        using MutableImage = ResLckOpt<dal::IImage, WriteLock>;
        MutableImage get_img_mut(const fs::path& path) override {
            auto item = this->get(path);
            if (!item)
                return std::nullopt;

            auto res_pair = item->writer_.get();
            if (!res_pair)
                return std::nullopt;

            auto adaptor = dynamic_cast<::ResAdaptor*>(res_pair->second);
            if (!adaptor)
                return std::nullopt;

            auto image = adaptor->get_img();
            if (!image)
                return std::nullopt;

            return std::make_pair(std::ref(*image), std::move(res_pair->first));
        }

        const dal::parser::Model* get_dmd(const fs::path& path) override {
            auto item = this->get(path);
            if (!item)
                return nullptr;

            auto res_pair = item->reader_.get();
            if (!res_pair)
                return nullptr;

            auto adaptor = dynamic_cast<const ::ResAdaptor*>(res_pair->second);
            if (!adaptor)
                return nullptr;

            return adaptor->get_dmd();
        }

    private:
        ResItem& get_or_create(const fs::path& path) {
            const auto path_str = path.u8string();
            auto it = items_.find(path_str);
            if (it != items_.end()) {
                return it->second;
            } else {
                auto it = items_.emplace(path_str, ResItem{});
                it.first->second.init(path, *task_sche_, *datacen_, filesys_);
                datacen_->get_writer_of(it.first->second.reader_).value();
                return it.first->second;
            }
        }

        ResItem* get(const fs::path& path) {
            const auto it = items_.find(path.u8string());
            if (it != items_.end()) {
                return &it->second;
            } else {
                return nullptr;
            }
        }

        std::shared_ptr<dal::Filesystem> filesys_;
        sung::HTaskSche task_sche_;
        sung::HDataCentral datacen_;
        std::unordered_map<std::string, ResItem> items_;
    };
    */

}  // namespace


namespace dal {

    HResMgr create_resmgr(HFilesys filesys, sung::HTaskSche task_sche) {
        return nullptr;
    }

}  // namespace dal
