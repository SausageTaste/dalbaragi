#pragma once

#include <mutex>
#include <shared_mutex>

#include <sung/basic/threading.hpp>

#include "daltools/filesys/filesys.hpp"
#include "daltools/img/img2d.hpp"
#include "daltools/scene/struct.h"


namespace dal {

    enum class ResType {
        img,
        dmd,
        unknown,
    };

    enum class ReqResult {
        ready,
        loading,
        cannot_read_file,
        not_supported_file,
        unknown_error,
    };


    class IResourceManager {

    public:
        using PTex = std::shared_ptr<dal::IImage2D>;
        using ReadLock = std::shared_lock<std::shared_mutex>;
        using WriteLock = std::unique_lock<std::shared_mutex>;

        template <typename TResType, typename TLockType>
        using ResLckOpt = std::optional<
            std::pair<std::reference_wrapper<TResType>, TLockType>>;

        virtual ~IResourceManager() = default;
        virtual ReqResult request(const fs::path& path) = 0;
        virtual ResType query_res_type(const fs::path& path) = 0;

        virtual ResLckOpt<const dal::IImage, ReadLock> get_img(
            const fs::path& path
        ) = 0;
        virtual ResLckOpt<dal::IImage, WriteLock> get_img_mut(
            const fs::path& path
        ) = 0;

        virtual const dal::Model* get_dmd(const fs::path& path) = 0;
    };

    using HResMgr = std::shared_ptr<IResourceManager>;
    HResMgr create_resmgr(HFilesys filesys, sung::HTaskSche task_sche);

}  // namespace dal
