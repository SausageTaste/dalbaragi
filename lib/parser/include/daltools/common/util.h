#pragma once

#include <filesystem>
#include <optional>
#include <vector>

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


    namespace fs = std::filesystem;

    std::optional<fs::path> find_git_repo_root(const fs::path& start_path);
    std::vector<uint8_t> read_file(const fs::path& path);


    class ValuesReport {

    public:
        using str = std::string_view;

        ValuesReport();
        ~ValuesReport();

        ValuesReport& set_title(str title);

        // Step by step
        ValuesReport& new_entry(str label);
        ValuesReport& new_entry(int indent, str label);
        ValuesReport& set_value(str v);
        ValuesReport& set_value(uint32_t v);

        // Append to last entry's value, with a comma
        ValuesReport& add_value(str v);
        ValuesReport& add_value(uint32_t v);

        // All in one
        ValuesReport& add(int indent, str label, str v);
        ValuesReport& add(int indent, str label, uint32_t v);
        ValuesReport& add(int indent, str label, uint64_t v);
        ValuesReport& add(int indent, str label, double v);
        ValuesReport& add(int indent, str label);

        ValuesReport& add(
            int indent, str label, const uint32_t* arr, size_t size
        );

        std::string build_str() const;

    private:
        struct Record;
        std::vector<Record> records_;
        std::string title_;
    };

}  // namespace dal
