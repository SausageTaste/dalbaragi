#include "work_functions.hpp"

#include <fstream>

#include <spdlog/fmt/fmt.h>
#include <argparse/argparse.hpp>

#include "dal/auxiliary/util.hpp"
#include "dal/parser/common/konst.hpp"


namespace dal {

    void work_keygen(int argc, char* argv[]) {
        /*
        sung::MonotonicRealtimeTimer timer;
        argparse::ArgumentParser parser{ "daltools" };
        {
            parser.add_argument("operation").help("Operation name");
            parser.add_argument("-o", "--output")
                .help("File path to save key files. Extension must be omitted")
                .required();
            parser.add_argument("--owner").required();
            parser.add_argument("--email").required();
            parser.add_argument("--description").default_value(std::string{});
        }
        parser.parse_args(argc, argv);

        const auto output_prefix = parser.get<std::string>("--output");

        dal::KeyMetadata md;
        md.owner_name_ = parser.get<std::string>("--owner");
        md.email_ = parser.get<std::string>("--email");
        md.description_ = parser.get<std::string>("--description");
        md.update_created_time();

        std::cout << "Keypair for data\n";
        timer.check();

        const auto [pk, sk] = dal::gen_data_key_pair();

        {
            const auto path = output_prefix + "-data_sec.dky";
            const auto data = dal::serialize_key(sk, md);
            std::ofstream file{ path };
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            fmt::print("    Secret key: {}\n", path);
        }

        {
            const auto path = output_prefix + "-data_pub.dky";
            const auto data = dal::serialize_key(pk, md);
            std::ofstream file{ path };
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            fmt::print("    Public key: {}\n", path);
        }

        fmt::print("    took {} seconds\n", timer.elapsed());
        */
    }

}  // namespace dal
