#include "daltools/common/util.h"

#include <fstream>
#include <sstream>
#include <thread>

#include <spdlog/fmt/fmt.h>


namespace {

    constexpr unsigned MISCROSEC_PER_SEC = 1000000;
    constexpr unsigned NANOSEC_PER_SEC = 1000000000;

    namespace chr = std::chrono;
    using SteadyClock = chr::steady_clock;


    void sleep_hot_until(const SteadyClock::time_point& until) {
        while (SteadyClock::now() < until) {
        }
    }

    void sleep_cold_until(const SteadyClock::time_point& until) {
        std::this_thread::sleep_until(until);
    }

    void sleep_hybrid_until(const SteadyClock::time_point& until) {
        const auto dur = until - SteadyClock::now();
        const auto dur_in_ms = chr::duration_cast<chr::microseconds>(dur);
        std::this_thread::sleep_for(chr::microseconds{ dur_in_ms.count() / 4 });
        ::sleep_hot_until(until);
    }

}  // namespace


// Header functions
namespace dal {

    std::optional<fs::path> find_git_repo_root(const fs::path& start_path) {
        auto current_dir = start_path;

        for (int i = 0; i < 10; ++i) {
            for (const auto& entry : fs::directory_iterator(current_dir)) {
                if (entry.path().filename().u8string() == ".git") {
                    return current_dir;
                }
            }
            current_dir = current_dir.parent_path();
        }

        return std::nullopt;
    }

    std::vector<uint8_t> read_file(const fs::path& path) {
        std::vector<uint8_t> content;

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return content;

        content.assign(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        );
        return content;
    }

}  // namespace dal


// TimerThatCaps
namespace dal {

    double TimerThatCaps::check_get_elapsed_cap_fps() {
        this->wait_to_cap_fps();
        return this->check_get_elapsed();
    }

    void TimerThatCaps::set_fps_cap(const uint32_t v) {
        if (0 != v)
            desired_delta_ = ::MISCROSEC_PER_SEC / v;
        else
            desired_delta_ = 0;
    }

    // Private

    void TimerThatCaps::wait_to_cap_fps() {
        const auto delta = std::chrono::microseconds{ desired_delta_ };
        const auto wake_time = this->last_checked() + delta;
        ::sleep_hybrid_until(wake_time);
    }

}  // namespace dal


// ValuesReport
namespace dal {

    struct ValuesReport::Record {
        size_t line_width(size_t label_width) const {
            return label_width + value_.size() + indent + 1;
        }

        std::string label_;
        std::string value_;
        int indent;
    };

    ValuesReport::ValuesReport() {}

    ValuesReport::~ValuesReport() {}

    ValuesReport& ValuesReport::set_title(str title) {
        title_ = title;
        return *this;
    }

    ValuesReport& ValuesReport::new_entry(str label) {
        records_.emplace_back();
        records_.back().label_ = label;
        return *this;
    }

    ValuesReport& ValuesReport::new_entry(int indent, str label) {
        records_.emplace_back();
        records_.back().indent = indent;
        records_.back().label_ = label;
        return *this;
    }

    ValuesReport& ValuesReport::set_value(str v) {
        records_.back().value_ = v;
        return *this;
    }

    ValuesReport& ValuesReport::set_value(uint32_t v) {
        return this->set_value(fmt::format("{}", v));
    }

    ValuesReport& ValuesReport::add_value(str v) {
        auto& r = records_.back();
        if (!r.value_.empty())
            r.value_ += ", ";
        r.value_ += v;
        return *this;
    }

    ValuesReport& ValuesReport::add_value(uint32_t v) {
        auto& r = records_.back();
        if (r.value_.empty())
            r.value_ += fmt::format("{}", v);
        else
            r.value_ += fmt::format(", {}", v);
        return *this;
    }

    ValuesReport& ValuesReport::add(int indent, str label, str value) {
        auto& r = records_.emplace_back();

        r.indent = indent;
        r.label_ = label;
        r.value_ = value;

        return *this;
    }

    ValuesReport& ValuesReport::add(int indent, str label, uint32_t value) {
        return this->add(indent, label, fmt::format("{}", value));
    }

    ValuesReport& ValuesReport::add(int indent, str label, uint64_t v) {
        return this->add(indent, label, fmt::format("{}", v));
    }

    ValuesReport& ValuesReport::add(
        int indent, str label, const uint32_t* arr, size_t size
    ) {
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < size; ++i) {
            ss << arr;
            if (i + 1 < size)
                ss << ", ";
        }
        ss << "]";

        return this->add(indent, label, ss.str());
    }

    ValuesReport& ValuesReport::add(int indent, str label, double value) {
        return this->add(indent, label, fmt::format("{:.2f}", value));
    }

    ValuesReport& ValuesReport::add(int indent, str label) {
        auto& r = records_.emplace_back();

        r.indent = indent;
        r.label_ = label;

        return *this;
    }

    std::string ValuesReport::build_str() const {
        std::stringstream ss;

        size_t label_w = 0;
        for (const auto& r : records_)
            label_w = std::max(label_w, r.label_.size() + r.indent);
        size_t line_width = 0;
        for (const auto& r : records_)
            line_width = std::max(line_width, label_w + r.value_.size() + 1);

        const std::string major_line(line_width, '=');
        const std::string minor_line(line_width, '-');

        ss << major_line << '\n';
        if (title_.empty())
            ss << "Report\n";
        else
            ss << title_ << '\n';
        ss << minor_line << '\n';

        for (const auto& r : records_) {
            if (r.indent > 0)
                ss << std::string(r.indent, ' ');

            if (r.value_.empty()) {
                ss << r.label_;
            } else {
                const auto spacing = label_w - r.indent - r.label_.size();
                ss << r.label_;
                ss << std::string(spacing + 1, ' ');
                ss << r.value_;
            }

            ss << '\n';
        }

        ss << major_line << '\n';
        return ss.str();
    }

}  // namespace dal
