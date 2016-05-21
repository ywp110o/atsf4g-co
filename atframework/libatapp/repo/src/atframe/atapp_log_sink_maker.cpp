

#include "atframe/atapp_log_sink_maker.h"


namespace atapp {
    log_sink_maker::log_sink_maker() {}

    log_sink_maker::~log_sink_maker() {}


    const std::string &log_sink_maker::get_file_sink_name() {
        static std::string ret = "file";
        return ret;
    }

    log_reg_t &log_sink_maker::get_file_sink_reg();

    const std::string &log_sink_maker::get_stdout_sink_name() {
        static std::string ret = "stdout";
        return ret;
    }

    log_reg_t &log_sink_maker::get_stdout_sink_reg();

    const std::string &log_sink_maker::get_stderr_sink_name() {
        static std::string ret = "stderr";
        return ret;
    }

    log_reg_t &log_sink_maker::get_stderr_sink_reg();
}
