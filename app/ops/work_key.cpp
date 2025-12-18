#include "work_functions.hpp"

#include <fstream>

#include <spdlog/fmt/chrono.h>
#include <spdlog/fmt/fmt.h>
#include <argparse/argparse.hpp>

#include "dal/auxiliary/util.hpp"


namespace {

    auto read_file(const std::filesystem::path& path) {
        std::ifstream file(path);
        std::string content;
        content.assign(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        );
        return content;
    }

}  // namespace


namespace dal {

    void work_key(int argc, char* argv[]) {
        /*
        namespace fs = std::filesystem;

        sung::MonotonicRealtimeTimer timer;
        argparse::ArgumentParser parser{ "daltools" };
        {
            parser.add_argument("operation").help("Operation name");
            parser.add_argument("files")
                .help("Input dky file paths")
                .remaining();
        }
        parser.parse_args(argc, argv);

        const auto files = parser.get<std::vector<std::string>>("files");
        for (const auto& x : files) {
            const fs::path key_path{ x };
            const auto data = ::read_file(key_path);
            const auto key_opt = dal::deserialize_key(data);
            if (!key_opt) {
                fmt::print("Invalid key file: '{}'\n", x);
                return;
            }

            const auto& [key, md] = key_opt.value();

            std::string key_type;
            if (key.index() == 0)
                key_type = "Data public";
            else if (key.index() == 1)
                key_type = "Data secret";
            else
                key_type = "Unknown";

            fmt::print("* {}\n", fs::absolute(key_path).u8string());
            fmt::print(" - Key type      {}\n", key_type);
            fmt::print(" - Owner         {}\n", md.owner_name_);
            fmt::print(" - E-mail        {}\n", md.email_);
            fmt::print(" - Created date  {}\n", md.created_time_);
            fmt::print(" - Description   {}\n", md.description_);
        }
        */
    }

}  // namespace dal
