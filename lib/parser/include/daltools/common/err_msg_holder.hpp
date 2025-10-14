#pragma once

#include <string>

#include <spdlog/fmt/fmt.h>


namespace dal::ts {

    class ErrorMsgHolder {

    public:
        bool is_done() const;
        bool has_succeeded() const;
        bool has_failed() const;
        const std::string& err_msg() const;

    protected:
        static void running();
        void success();
        void fail(const char* err_msg);
        void fail(const std::string& err_msg);
        void fail(std::string&& err_msg);

        template <typename... T>
        auto fail(fmt::format_string<T...> fmt, T&&... args) {
            return this->fail(vformat(fmt, fmt::make_format_args(args...)));
        }

    private:
        std::string err_msg_;
        bool done_ = false;
    };

}  // namespace dal::ts
