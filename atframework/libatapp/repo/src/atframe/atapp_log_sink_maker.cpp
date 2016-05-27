#include <iostream>

#include "cli/shell_font.h"
#include "log/log_sink_file_backend.h"

#include "atframe/atapp_log_sink_maker.h"


namespace atapp {
    namespace detail {
        static util::log::log_wrapper::log_handler_t _log_sink_file(const std::string &sink_name, util::log::log_wrapper &logger,
                                                                    uint32_t index, util::config::ini_value &ini_cfg) {
            util::log::log_sink_file_backend file_sink;
            file_sink.set_file_pattern("");
            file_sink.set_max_file_size(65536);
            file_sink.set_rotate_size(10);
            file_sink.set_auto_flush(false);
            file_sink.set_check_interval(60);
        }

        static void _log_sink_stdout_handle(const util::log::log_wrapper::caller_info_t &caller, const char *content, size_t content_size) {
            std::cout.write(content, content_size);
            std::cout << std::endl;
        }

        static util::log::log_wrapper::log_handler_t _log_sink_stdout(const std::string &sink_name, util::log::log_wrapper &logger,
                                                                      uint32_t index, util::config::ini_value &ini_cfg) {
            return _log_sink_stdout_handle;
        }

        static void _log_sink_stderr_handle(const util::log::log_wrapper::caller_info_t &caller, const char *content, size_t content_size) {
            util::cli::shell_stream ss(std::cerr);
            ss() << util::cli::shell_font_style::SHELL_FONT_COLOR_RED << content << std::endl;
        }
        static util::log::log_wrapper::log_handler_t _log_sink_stderr(const std::string &sink_name, util::log::log_wrapper &logger,
                                                                      uint32_t index, util::config::ini_value &ini_cfg) {
            return _log_sink_stderr_handle;
        }
    }

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
