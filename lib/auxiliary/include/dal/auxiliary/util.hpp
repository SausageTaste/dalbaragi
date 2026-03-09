#pragma once

#include <filesystem>
#include <format>
#include <optional>
#include <vector>


namespace dal {

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

        template <typename T>
        ValuesReport& set_value(T v) {
            return this->set_value_str(std::format("{}", v));
        }

        // Append to last entry's value, with a comma
        ValuesReport& add_value(str v);
        ValuesReport& add_value(uint32_t v);

        // All in one
        template <typename T>
        ValuesReport& add(int indent, str label, const T& v) {
            return this->new_entry(indent, label).set_value(v);
        }

        ValuesReport& add(int indent, str label);
        ValuesReport& add(
            int indent, str label, const uint32_t* arr, size_t size
        );

        std::string build_str() const;

    private:
        ValuesReport& set_value_str(str v);

        struct Record;
        std::vector<Record> records_;
        std::string title_;
    };

}  // namespace dal
