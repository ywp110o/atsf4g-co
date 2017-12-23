#include <algorithm/base64.h>

#include "etcd_cluster.h"
#include "etcd_keepalive.h"


namespace atframe {
    namespace component {
        etcd_keepalive::default_checker_t::default_checker_t(const std::string &checked) {
            // encode into base64
            util::base64_encode(data, checked);
        }

        bool etcd_keepalive::default_checker_t::operator()(const std::string &checked) const {
            // both is base64
            return data == checked;
        }

        etcd_keepalive::etcd_keepalive(etcd_cluster &owner, const std::string &path, constrict_helper_t &) : owner_(&owner) {
            checker_.is_auto_decode = true;
            checker_.is_check_run = false;
            checker_.is_check_passed = false;
            rpc_.is_actived = false;

            util::base64_encode(path_, path);
        }

        etcd_keepalive::ptr_t etcd_keepalive::create(etcd_cluster &owner, const std::string &path) {
            constrict_helper_t h;
            return std::make_shared<etcd_keepalive>(owner, path, h);
        }

        void etcd_keepalive::set_checker(const std::string &checked_str) {
            checker_.fn = default_checker_t(checked_str);
            checker_.is_auto_decode = false;
        }

        void etcd_keepalive::set_checker(checker_fn_t fn, bool auto_decode) {
            checker_.fn = fn;
            checker_.is_auto_decode = auto_decode;
        }

        std::string etcd_keepalive::get_path() const {
            std::string ret;
            util::base64_decode(ret, path_);

            // these should be optimization by NRVO
            return ret;
        }

        void etcd_keepalive::active() {
            rpc_.is_actived = true;
            process();
        }

        void etcd_keepalive::process() {
            if (rpc_.rpc_opr_) {
                return;
            }

            rpc_.is_actived = false;

            // if has checker and has not check date yet, send a check request
            if (false == checker_.is_check_run && checker_.fn) {
                // create a check rpc
            } else {
                checker_.is_check_run = true;
                checker_.is_check_passed = true;
            }

            // if check passed, set data
            if (checker_.is_check_run && checker_.is_check_passed) {
                // TODO create set data rpc
            }
        }

    } // namespace component
} // namespace atframe
