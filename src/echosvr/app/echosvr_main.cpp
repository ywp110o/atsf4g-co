
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>

#include <atframe/atapp.h>
#include <time/time_utility.h>

#include <libatgw_server_protocol.h>
#include <config/atframe_service_types.h>


typedef std::map<uint64_t, uint64_t> session_gw_map_t;

struct app_command_handler_kickoff {
    atapp::app* app_;
    session_gw_map_t* gw_;
    app_command_handler_kickoff(atapp::app* app, session_gw_map_t* gw) :app_(app), gw_(gw){}
    int operator()(util::cli::callback_param params) {
        if (params.get_params_number() <= 0) {
            WLOGERROR("kickoff command must require session id");
            return 0;
        }

        uint64_t sess_id = params[0]->to_uint64();
        session_gw_map_t::iterator iter = gw_->find(sess_id);
        if (iter == gw_->end()) {
            WLOGWARNING("try to kickoff 0x%llx, but session not found", sess_id);
            return 0;
        } else {
            WLOGINFO("kickoff 0x%llx", sess_id);
        }

        ::atframe::gw::ss_msg msg;
        msg.init(ATFRAME_GW_CMD_SESSION_KICKOFF, sess_id);

        std::stringstream ss;
        msgpack::pack(ss, msg);
        std::string packed_buffer;
        ss.str().swap(packed_buffer);

        return app_->get_bus_node()->send_data(iter->second, 0, packed_buffer.data(), packed_buffer.size());
    }
};

struct app_handle_on_msg {
    session_gw_map_t* gw_;
    app_handle_on_msg(session_gw_map_t* gw) :gw_(gw) {}

    int operator()(atapp::app &app, const atapp::app::msg_t& msg, const void *buffer, size_t len) {
        if (NULL == msg.body.forward || 0 == msg.head.src_bus_id) {
            WLOGERROR("receive a message from unknown source");
            return app.get_bus_node()->send_data(msg.head.src_bus_id, msg.head.type, buffer, len);
        }

        switch (msg.head.type) {
        case ::atframe::component::service_type::EN_ATST_GATEWAY: {
            ::atframe::gw::ss_msg req_msg;
            msgpack::unpacked result;
            msgpack::unpack(result, reinterpret_cast<const char *>(buffer), len);
            msgpack::object obj = result.get();
            if (obj.is_nil()) {
                return 0;
            }
            obj.convert(req_msg);

            switch (req_msg.head.cmd) {
            case ATFRAME_GW_CMD_POST: {
                // keep all data not changed and send back
                int res = app.get_bus_node()->send_data(msg.body.forward->from, 0, buffer, len);
                if (res < 0) {
                    WLOGERROR("send back post data to 0x%llx failed, res: %d", msg.body.forward->from, res);
                } else if (NULL != req_msg.body.post) {
                    WLOGDEBUG("receive msg %s and send back to 0x%llx done", 
                        std::string(reinterpret_cast<const char*>(req_msg.body.post->content.ptr),
                            req_msg.body.post->content.size).c_str(), msg.body.forward->from
                    );
                }
                break;
            }
            case ATFRAME_GW_CMD_SESSION_ADD: {
                WLOGINFO("create new session 0x%llx, address: %s:%d",
                    req_msg.head.session_id,
                    req_msg.body.session->client_ip.c_str(),
                    req_msg.body.session->client_port
                );

                if (0 != req_msg.head.session_id) {
                    (*gw_)[req_msg.head.session_id] = msg.body.forward->from;
                }
                break;
            }
            case ATFRAME_GW_CMD_SESSION_REMOVE: {
                WLOGINFO("remove session 0x%llx",
                    req_msg.head.session_id
                );

                gw_->erase(req_msg.head.session_id);
                break;
            }
            default:
                WLOGERROR("receive a unsupport atgateway message of invalid cmd:%d", static_cast<int>(req_msg.head.cmd));
                break;
            }

            break;
        }

        default:
            WLOGERROR("receive a message of invalid type:%d", msg.head.type);
            break;
        }

        return 0;
    }
};

static int app_handle_on_send_fail(atapp::app &app, atapp::app::app_id_t src_pd, atapp::app::app_id_t dst_pd,
    const atbus::protocol::msg &m) {
    WLOGERROR("send data from 0x%llx to 0x%llx failed", src_pd, dst_pd);
    return 0;
}

static int app_handle_on_connected(atapp::app &app, atbus::endpoint &ep, int status) {
    WLOGINFO("app 0x%llx connected, status: %d", ep.get_id(), status);
    return 0;
}

static int app_handle_on_disconnected(atapp::app &app, atbus::endpoint &ep, int status) {
    WLOGINFO("app 0x%llx disconnected, status: %d", ep.get_id(), status);
    return 0;
}

int main(int argc, char *argv[]) {
    atapp::app app;
    session_gw_map_t gws;

    // setup cmd
    util::cli::cmd_option_ci::ptr_type cmgr = app.get_command_manager();
    cmgr->bind_cmd("kickoff", app_command_handler_kickoff(&app, &gws))
        ->set_help_msg("kickoff <session id>                   kickoff a client.");

    // setup message handle
    app.set_evt_on_recv_msg(app_handle_on_msg(&gws));
    app.set_evt_on_send_fail(app_handle_on_send_fail);
    app.set_evt_on_app_connected(app_handle_on_connected);
    app.set_evt_on_app_disconnected(app_handle_on_disconnected);

    // run
    return app.run(uv_default_loop(), argc, (const char **)argv, NULL);
}
