#pragma once

#include <sung/basic/time.hpp>


namespace dal {

    class TimerThatCaps : public sung::MonotonicRealtimeTimer {

    public:
        double check_get_elapsed_cap_fps();
        void set_fps_cap(const uint32_t v);

    private:
        void wait_to_cap_fps();

        uint32_t desired_delta_ = 0;  // In microseconds
    };


}  // namespace dal
