#pragma once

#include <enkiTS/TaskScheduler.h>


namespace dal {

    enki::TaskScheduler& tasker();


    class ITask : private enki::ITaskSet {

    public:
        void set_size(uint32_t count) { m_SetSize = count; }

        void submit(enki::TaskScheduler& scheduler) {
            scheduler.AddTaskSetToPipe(static_cast<enki::ITaskSet*>(this));
        }

        void submit() { return this->submit(dal::tasker()); }

        void set_dep(std::unique_ptr<ITask>&& task) {
            task->SetDependency(task->deps_, this);
            sub_tasks_.push_back(std::move(task));
        }

    private:
        enki::Dependency deps_;
        std::vector<std::unique_ptr<ITask>> sub_tasks_;
    };

}  // namespace dal
