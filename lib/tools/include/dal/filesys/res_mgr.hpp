#pragma once


namespace dal {

    enum class ReqResult {
        ready,
        loading,
        cannot_read_file,
        not_supported_file,
        unknown_error,
    };

}  // namespace dal
