#include "dal/auxiliary/util.hpp"

#include <fstream>
#include <sstream>
#include <thread>

#include <spdlog/fmt/fmt.h>

#include "dal/auxiliary/path.hpp"


// Header functions
namespace dal {

    std::optional<fs::path> find_git_repo_root(const fs::path& start_path) {
        auto current_dir = start_path;

        for (int i = 0; i < 10; ++i) {
            for (const auto& entry : fs::directory_iterator(current_dir)) {
                if (dal::tostr(entry.path().filename()) == ".git") {
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

    ValuesReport& ValuesReport::set_value_str(str v) {
        records_.back().value_ = v;
        return *this;
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
