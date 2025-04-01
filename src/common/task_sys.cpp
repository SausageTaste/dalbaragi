#include "daltools/common/task_sys.hpp"


namespace {

    class TaskScheRaii {

    public:
        TaskScheRaii() { ts_.Initialize(); }

        auto& get() { return ts_; }

    private:
        enki::TaskScheduler ts_;
    };

}  // namespace


namespace dal::ts {

    enki::TaskScheduler& inst() {
        static TaskScheRaii inst;
        return inst.get();
    }

}  // namespace dal::ts
