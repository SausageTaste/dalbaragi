#include "work_functions.hpp"

#include <spdlog/fmt/chrono.h>
#include <spdlog/fmt/fmt.h>
#include <argparse/argparse.hpp>

#include "daltools/common/crypto.h"
#include "daltools/common/util.h"


namespace dal {

    void work_key(int argc, char* argv[]) {
        dal::Timer timer;
        argparse::ArgumentParser parser{ "daltools" };
        {
        parser.add_argument("operation")
            .help("Operation name");

        parser.add_argument("-p", "--print")
            .help("Print attributes of a key")
            .default_value(false)
            .implicit_value(true);

        parser.add_argument("-i", "--input")
            .help("Key path")
            .required();
        }
        parser.parse_args(argc, argv);

        const auto key_path = parser.get<std::string>("--input");
        const auto [key, attrib] = dal::crypto::load_key<dal::crypto::IKey>(key_path.c_str());

        if (parser["--print"] == true) {
            fmt::print("Owner: {}\n", attrib.m_owner_name);
            fmt::print("E-mail: {}\n", attrib.m_email);
            fmt::print("Description: {}\n", attrib.m_description);
            fmt::print("Key type: {}\n", attrib.get_type_str());

            const auto a = std::chrono::system_clock::to_time_t(attrib.m_created_time);
            // fmt::print("Created date: {:%F %T %z}\n", fmt::localtime(a));
            std::cout << "===================================\n";
        }
    }

}
