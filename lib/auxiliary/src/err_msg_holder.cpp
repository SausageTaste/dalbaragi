#include "dal/auxiliary/err_msg_holder.hpp"


// ErrorMsgHolder
namespace dal {

    bool ErrorMsgHolder::is_done() const { return done_; }

    bool ErrorMsgHolder::has_succeeded() const {
        return done_ && err_msg_.empty();
    }

    bool ErrorMsgHolder::has_failed() const {
        return done_ && !err_msg_.empty();
    }

    const std::string& ErrorMsgHolder::err_msg() const { return err_msg_; }

    void ErrorMsgHolder::running() { return; }

    void ErrorMsgHolder::success() {
        err_msg_.clear();
        done_ = true;
    }

    void ErrorMsgHolder::fail(const char* err_msg) {
        err_msg_ = err_msg;
        done_ = true;
    }

    void ErrorMsgHolder::fail(const std::string& err_msg) {
        err_msg_ = err_msg;
        done_ = true;
    }

    void ErrorMsgHolder::fail(std::string&& err_msg) {
        err_msg_ = std::move(err_msg);
        done_ = true;
    }

}  // namespace dal
