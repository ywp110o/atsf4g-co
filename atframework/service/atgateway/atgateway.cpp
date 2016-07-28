
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <std/ref.h>
#include <vector>

#include <uv.h>

#include <atframe/atapp.h>
#include <session_manager.h>
#include <time/time_utility.h>


static int app_handle_on_send_fail(atapp::app &app, atapp::app::app_id_t src_pd, atapp::app::app_id_t dst_pd,
                                   const atbus::protocol::msg &m) {
    WLOGERROR("send data from 0x%llx to 0x%llx failed", src_pd, dst_pd);
    return 0;
}

class gateway_module : public ::atapp::module_impl {
public:
    gateway_module();
    virtual ~gateway_module();

public:
    virtual int init() CLASS_OVERRIDE {
        int res = 0;
        if ("inner" == gw_mgr_.get_conf().listen.type) {
            gw_mgr_.init(get_app()->get_bus_node(), std::bind(&gateway_module::create_proto_inner, this));
            // proto_callbacks_.write_fn = ;
            // proto_callbacks_.message_fn = ;
            // proto_callbacks_.new_session_fn = ;
            // proto_callbacks_.reconnect_fn = ;
            // proto_callbacks_.close_fn = ;

        } else {
            fprintf(stderr, "listen type %s not supported\n", gw_mgr_.get_conf().listen.type.c_str());
            return -1;
        }

        res = gw_mgr_.listen_all();
        if (res <= 0) {
            fprintf(stderr, "nothing listened for client\n");
            return -1;
        }

        return 0;
    }

    virtual int reload() CLASS_OVERRIDE {
        // TODO configure
    }

    virtual int stop() CLASS_OVERRIDE {
        gw_mgr_.reset();
        return 0;
    }

    virtual int timeout() CLASS_OVERRIDE { return 0; }

    virtual const char *name() const CLASS_OVERRIDE { return "gateway_module"; }

    virtual int tick() CLASS_OVERRIDE { return gw_mgr_.tick(); }

    inline ::atframe::gateway::session_manager &get_session_manager() { return gw_mgr_; }
    inline const ::atframe::gateway::session_manager &get_session_manager() const { return gw_mgr_; }

private:
    std::unique_ptr< ::atframe::gateway::proto_base> create_proto_inner() {
        ::atframe::gateway::libatgw_proto_inner_v1 *ret = new (std::nothrow)::atframe::gateway::libatgw_proto_inner_v1();
        if (NULL != ret) {
            ret->set_callbacks(&proto_callbacks_);

            // TODO setup crypt option

            // TODO setup zip option
        }

        return std::unique_ptr< ::atframe::gateway::proto_base>(ret);
    }

private:
    ::atframe::gateway::session_manager gw_mgr_;
    ::atframe::gateway::proto_base::proto_callbacks_t proto_callbacks_;
};

struct app_handle_on_recv {
    std::reference_wrapper<gateway_module> mod_;
    app_handle_on_recv(gateway_module &mod) : mod_(mod) {}

    int operator()(atapp::app &app, const atbus::app::msg_head_t *header, const void *buffer, size_t len) {
        if (NULL == buffer || 0 == len) {
            return 0;
        }
        ::atframe::gw::ss_msg msg;
        assert(header);

        msgpack::unpacked result;
        msgpack::unpack(&result, reinterpret_cast<const char *>(buffer), len);
        msgpack::object obj = result->get();
        if (obj.is_nil()) {
            return 0;
        }
        obj.convert(msg);

        switch (msg.head.cmd) {
        case ATFRAME_GW_CMD_POST: {
            if (NULL == msg.body.post) {
                WLOGERROR("from server 0x%llx: recv bad post body", header->src_bus_id);
                break;
            }

            // post to single client
            if (0 != msg.head.session_id && msg.body.post->session_ids.empty()) {
                WLOGDEBUG("from server 0x%llx: session 0x%llx send %llu bytes data to client", header->src_bus_id, msg.head.session_id,
                          static_cast<unsigned long long>(msg.body.post->content.size));

                int res =
                    mod_.get_session_manager().push_data(msg.head.session_id, msg.body.post->content.ptr, msg.body.post->content.size);
                if (0 != res) {
                    WLOGERROR("from server 0x%llx: session 0x%llx push data failed, res: %d ", header->src_bus_id, msg.head.session_id,
                              res);
                }
            } else if (msg.body.post->session_ids.empty()) { // broadcast to all actived session
                int res = mod_.get_session_manager().broadcast_data(msg.body.post->content.ptr, msg.body.post->content.size);
                if (0 != res) {
                    WLOGERROR("from server 0x%llx: broadcast data failed, res: %d ", header->src_bus_id, res);
                }
            } else { // multicast to more than one client
                for (std::vector<uint64_t>::iterator iter = msg.body.post->session_ids.begin(); iter != msg.body.post->session_ids.end();
                     ++iter) {
                    int res = mod_.get_session_manager().push_data(*iter, msg.body.post->content.ptr, msg.body.post->content.size);
                    if (0 != res) {
                        WLOGERROR("from server 0x%llx: session 0x%llx push data failed, res: %d ", header->src_bus_id, *iter, res);
                    }
                }
            }
            break;
        }
        case ATFRAME_GW_CMD_SESSION_KICKOFF: {
            WLOGINFO("from server 0x%llx: session 0x%llx kickoff by server", header->src_bus_id, msg.head.session_id);
            mod_.get_session_manager().close(msg.head.session_id, ::atframe::gateway::close_reason_t::EN_CRT_KICKOFF);
            break;
        }
        case ATFRAME_GW_CMD_SET_ROUTER_REQ: {
            int res = mod_.get_session_manager().set_session_router(msg.head.session_id, msg.body.router);
            WLOGINFO("from server 0x%llx: session 0x%llx set router to %0xllx by server, res: %d", header->src_bus_id, msg.head.session_id,
                     msg.body.router, res);

            ::atframe::gw::ss_msg rsp;
            rsp.init(ATFRAME_GW_CMD_SET_ROUTER_RSP, msg.head.session_id);
            rsp.head.error_code = res;

            res = mod_.get_session_manager().post_data(header->src_bus_id, rsp);
            if (0 != res) {
                WLOGERROR("send set router response to server 0x%llx failed, res: %d", header->src_bus_id, res);
            }
            break;
        }
        default: {
            WLOGERROR("from server 0x%llx: session 0x%llx recv invalid cmd %d", header->src_bus_id, msg.head.session_id,
                      static_cast<int>(msg.head.cmd));
            break;
        }
        }
        return 0;
    }
};

int main(int argc, char *argv[]) {
    atapp::app app;
    std::shared_ptr<gateway_module> gw_mod = std::make_shared<gateway_module>();
    if (!gw_mod) {
        fprintf(stderr, "create gateway module failed\n");
        return -1;
    }

    // setup module
    app.add_module(gw_mod);

    // setup message handle
    app.set_evt_on_send_fail(app_handle_on_send_fail);
    app.set_evt_on_recv_msg(app_handle_on_recv(*gw_mod));

    // run
    return app.run(uv_default_loop(), argc, (const char **)argv, NULL);
}
