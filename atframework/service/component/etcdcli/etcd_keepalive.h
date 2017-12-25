#include <string>

#include <std/functional.h>
#include <std/smart_ptr.h>

#include <config/compiler_features.h>

#include <network/http_request.h>


namespace atframe {
    namespace component {
        class etcd_cluster;

        class etcd_keepalive {
        public:
            typedef std::function<bool(const std::string &)> checker_fn_t; // the parameter will be base64 of the value
            typedef std::shared_ptr<etcd_keepalive> ptr_t;

            struct default_checker_t {
                default_checker_t(const std::string &checked);

                bool operator()(const std::string &checked) const;

                std::string data;
            };

        private:
            struct constrict_helper_t {};

        public:
            etcd_keepalive(etcd_cluster &owner, const std::string &path, constrict_helper_t &helper);
            static ptr_t create(etcd_cluster &owner, const std::string &path);

            void close();

            void set_checker(const std::string &checked_str);
            void set_checker(checker_fn_t fn);

            inline void set_value(const std::string &str) { value_ = str; }
#if defined(UTIL_CONFIG_COMPILER_CXX_RVALUE_REFERENCES) && UTIL_CONFIG_COMPILER_CXX_RVALUE_REFERENCES
            inline void set_value(std::string &&str) { value_.swap(str); }
#endif
            inline const std::string &get_value() const { return value_; }

            std::string get_path() const;

            void active();

            etcd_cluster &get_owner() { return *owner_; }
            const etcd_cluster &get_owner() const { return *owner_; }

        private:
            void process();

        private:
            static int libcurl_callback_on_get_data(util::network::http_request &req);
            static int libcurl_callback_on_set_data(util::network::http_request &req);

        private:
            etcd_cluster *owner_;
            std::string path_;
            std::string value_;
            typedef struct {
                util::network::http_request::ptr_t rpc_opr_;
                bool is_actived;
            } rpc_data_t;
            rpc_data_t rpc_;

            typedef struct {
                checker_fn_t fn;
                bool is_check_run;
                bool is_check_passed;
            } checker_t;
            checker_t checker_;
        };
    } // namespace component
} // namespace atframe