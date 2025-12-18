#include "dal/parser/common/task_sys.hpp"


namespace {

    class TaskScheRaii {

    public:
        TaskScheRaii() { ts_.Initialize(); }

        auto& get() { return ts_; }

    private:
        enki::TaskScheduler ts_;
    };

}  // namespace


namespace dal {

    enki::TaskScheduler& tasker() {
        static TaskScheRaii inst;
        return inst.get();
    }

}  // namespace dal
