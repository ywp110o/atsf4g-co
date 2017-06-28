//
// Created by owt50 on 2016/9/27.
//

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#include <WinSock2.h>
#endif

#include <log/log_wrapper.h>

#include <atframe/atapp.h>
#include <config/atframe_service_types.h>
#include <libatgw_server_protocol.h>
#include <proto_base.h>

#include <protocol/pbdesc/svr.const.err.pb.h>
#include <protocol/pbdesc/svr.container.pb.h>


#include <logic/action/task_action_player_logout.h>
#include <logic/session_manager.h>

#include "cs_msg_dispatcher.h"
#include "task_manager.h"


cs_msg_dispatcher::cs_msg_dispatcher() {}
cs_msg_dispatcher::~cs_msg_dispatcher() {}

const char *cs_msg_dispatcher::name() const { return "cs_msg_dispatcher"; }

int32_t cs_msg_dispatcher::init() { return 0; }

int32_t cs_msg_dispatcher::unpack_msg(msg_ptr_t msg_container, const void *msg_buf, size_t msg_size) {
    if (NULL == msg_container) {
        WLOGERROR("parameter error");
        return hello::err::EN_SYS_PARAM;
    }

    if (false == msg_container->mutable_csmsg()->ParseFromArray(msg_buf, static_cast<int>(msg_size))) {
        WLOGERROR("unpack msg failed\n%s", msg_container->mutable_csmsg()->InitializationErrorString().c_str());
        return hello::err::EN_SYS_UNPACK;
    }

    return hello::err::EN_SUCCESS;
}

uint64_t cs_msg_dispatcher::pick_msg_task(const msg_ptr_t msg_container) {
    // cs msg not allow resume task
    return 0;
}

const std::string &cs_msg_dispatcher::pick_msg_name(const msg_ptr_t msg_container) {
    if (NULL == msg_container) {
        return get_empty_string();
    }

    if (false == msg_container->has_csmsg()) {
        return get_empty_string();
    }

    if (false == msg_container->csmsg().has_body()) {
        return get_empty_string();
    }

    std::vector<const google::protobuf::FieldDescriptor *> output;
    msg_container->csmsg().body().GetReflection()->ListFields(msg_container->csmsg().body(), &output);
    if (output.empty()) {
        return get_empty_string();
    }

    if (output.size() > 1) {
        WLOGERROR("there is more than one body");
        for (size_t i = 0; i < output.size(); ++i) {
            WLOGERROR("body[%d]=%s", static_cast<int>(i), output[i]->name().c_str());
        }
    }

    return output[0]->name();
}

cs_msg_dispatcher::msg_type_t cs_msg_dispatcher::pick_msg_type_id(const msg_ptr_t msg_container) {
    if (NULL == msg_container) {
        return 0;
    }

    if (false == msg_container->has_csmsg()) {
        return 0;
    }

    if (false == msg_container->csmsg().has_body()) {
        return 0;
    }

    std::vector<const google::protobuf::FieldDescriptor *> output;
    msg_container->csmsg().body().GetReflection()->ListFields(msg_container->csmsg().body(), &output);
    if (output.empty()) {
        return 0;
    }

    if (output.size() > 1) {
        WLOGERROR("there is more than one body");
        for (size_t i = 0; i < output.size(); ++i) {
            WLOGERROR("body [%d]=%s", static_cast<int>(i), output[i]->name().c_str());
        }
    }

    return static_cast<msg_type_t>(output[0]->number());
}

cs_msg_dispatcher::msg_type_t cs_msg_dispatcher::msg_name_to_type_id(const std::string &msg_name) {
    const google::protobuf::FieldDescriptor *desc = hello::CSMsgBody::descriptor()->FindFieldByName(msg_name);
    if (NULL == desc) {
        return 0;
    }

    return static_cast<msg_type_t>(desc->number());
}

int32_t cs_msg_dispatcher::dispatch(const atbus::protocol::msg &msg, const void *buffer, size_t len) {
    if (::atframe::component::service_type::EN_ATST_GATEWAY != msg.head.type) {
        WLOGERROR("message type %d invalid", msg.head.type);
        return hello::err::EN_SYS_PARAM;
    }

    if (NULL == msg.body.forward || 0 == msg.head.src_bus_id) {
        WLOGERROR("receive a message from unknown source");
        return hello::err::EN_SYS_PARAM;
    }

    ::atframe::gw::ss_msg req_msg;
    msgpack::unpacked result;
    msgpack::unpack(result, reinterpret_cast<const char *>(buffer), len);
    msgpack::object obj = result.get();
    if (obj.is_nil()) {
        return 0;
    }
    obj.convert(req_msg);

    int ret = hello::err::EN_SUCCESS;
    switch (req_msg.head.cmd) {
    case ATFRAME_GW_CMD_POST: {
        hello::message_container cs_msg;
        session::key_t session_key;
        session_key.bus_id = msg.body.forward->from;
        session_key.session_id = req_msg.head.session_id;
        cs_msg.mutable_src_server()->set_bus_id(session_key.bus_id);
        cs_msg.mutable_src_client()->set_session_id(session_key.session_id);

        if (!session_manager::me()->find(session_key)) {
            WLOGERROR("session [0x%llx, 0x%llx] not found, try to kickoff", static_cast<unsigned long long>(session_key.bus_id),
                      static_cast<unsigned long long>(session_key.session_id));
            ret = hello::err::EN_SYS_NOTFOUND;

            send_kickoff(session_key.bus_id, session_key.session_id, hello::EN_CRT_SESSION_NOT_FOUND);
            break;
        }

        ret = on_recv_msg(&cs_msg, req_msg.body.post->content.ptr, req_msg.body.post->content.size);
        if (ret < 0) {
            WLOGERROR("on receive message callback from to 0x%llx failed, res: %d", static_cast<unsigned long long>(msg.body.forward->from), ret);
        }
        break;
    }
    case ATFRAME_GW_CMD_SESSION_ADD: {
        session::key_t session_key;
        session_key.bus_id = msg.body.forward->from;
        session_key.session_id = req_msg.head.session_id;

        WLOGINFO("create new session [0x%llx, 0x%llx], address: %s:%d", static_cast<unsigned long long>(session_key.bus_id),
                 static_cast<unsigned long long>(session_key.session_id), req_msg.body.session->client_ip.c_str(), req_msg.body.session->client_port);

        session_manager::sess_ptr_t sess = session_manager::me()->create(session_key);
        if (!sess) {
            WLOGERROR("malloc failed");
            ret = hello::err::EN_SYS_MALLOC;
            send_kickoff(session_key.bus_id, session_key.session_id, ::atframe::gateway::close_reason_t::EN_CRT_SERVER_BUSY);
            break;
        }

        break;
    }
    case ATFRAME_GW_CMD_SESSION_REMOVE: {
        session::key_t session_key;
        session_key.bus_id = msg.body.forward->from;
        session_key.session_id = req_msg.head.session_id;

        WLOGINFO("remove session [0x%llx, 0x%llx]", static_cast<unsigned long long>(session_key.bus_id),
                 static_cast<unsigned long long>(session_key.session_id));

        // logout task
        task_manager::id_t logout_task_id = 0;
        ret = task_manager::me()->create_task<task_action_player_logout>(logout_task_id, session_key.bus_id, session_key.session_id);
        if (0 == ret) {
            hello::message_container cs_msg;
            cs_msg.mutable_src_server()->set_bus_id(session_key.bus_id);
            cs_msg.mutable_src_client()->set_session_id(session_key.session_id);
            ret = task_manager::me()->start_task(logout_task_id, cs_msg);

            if (0 != ret) {
                WLOGERROR("run logout task failed, res: %d", ret);
                session_manager::me()->remove(session_key);
            }
        } else {
            WLOGERROR("create logout task failed, res: %d", ret);
            session_manager::me()->remove(session_key);
        }
        break;
    }
    default:
        WLOGERROR("receive a unsupport atgateway message of invalid cmd:%d", static_cast<int>(req_msg.head.cmd));
        break;
    }

    return ret;
}

int32_t cs_msg_dispatcher::send_kickoff(uint64_t bus_id, uint64_t session_id, int32_t reason) {
    atapp::app *owner = get_app();
    if (NULL == owner) {
        WLOGERROR("not in a atapp");
        return hello::err::EN_SYS_INIT;
    }

    ::atframe::gw::ss_msg msg;
    msg.init(ATFRAME_GW_CMD_SESSION_KICKOFF, session_id);
    msg.head.error_code = reason;

    std::stringstream ss;
    msgpack::pack(ss, msg);
    std::string packed_buffer;
    ss.str().swap(packed_buffer);

    return owner->get_bus_node()->send_data(bus_id, 0, packed_buffer.data(), packed_buffer.size());
}

int32_t cs_msg_dispatcher::send_data(uint64_t bus_id, uint64_t session_id, const void *buffer, size_t len) {
    atapp::app *owner = get_app();
    if (NULL == owner) {
        WLOGERROR("not in a atapp");
        return hello::err::EN_SYS_INIT;
    }

    ::atframe::gw::ss_msg msg;
    msg.init(ATFRAME_GW_CMD_POST, session_id);
    msg.body.make_post(buffer, len);
    if (NULL == msg.body.post) {
        if (0 == session_id) {
            WLOGERROR("broadcast data to atgateway 0x%llx failed when malloc post", static_cast<unsigned long long>(bus_id));
        } else {
            WLOGERROR("send data to session [0x%llx, 0x%llx] failed when malloc post", static_cast<unsigned long long>(bus_id),
                      static_cast<unsigned long long>(session_id));
        }
        return hello::err::EN_SYS_MALLOC;
    }

    std::stringstream ss;
    msgpack::pack(ss, msg);
    std::string packed_buffer;
    ss.str().swap(packed_buffer);

    int ret = owner->get_bus_node()->send_data(bus_id, ::atframe::component::service_type::EN_ATST_GATEWAY, packed_buffer.data(), packed_buffer.size());
    if (ret < 0) {
        if (0 == session_id) {
            WLOGERROR("broadcast data to atgateway 0x%llx failed, res: %d", static_cast<unsigned long long>(bus_id), ret);
        } else {
            WLOGERROR("send data to session [0x%llx, 0x%llx] failed, res: %d", static_cast<unsigned long long>(bus_id),
                      static_cast<unsigned long long>(session_id), ret);
        }
    }

    return ret;
}

int32_t cs_msg_dispatcher::broadcast_data(uint64_t bus_id, const void *buffer, size_t len) { return send_data(bus_id, 0, buffer, len); }

int32_t cs_msg_dispatcher::broadcast_data(uint64_t bus_id, const std::vector<uint64_t> &session_ids, const void *buffer, size_t len) {
    atapp::app *owner = get_app();
    if (NULL == owner) {
        WLOGERROR("not in a atapp");
        return hello::err::EN_SYS_INIT;
    }

    ::atframe::gw::ss_msg msg;
    msg.init(ATFRAME_GW_CMD_POST, 0);
    msg.body.make_post(buffer, len);
    if (NULL == msg.body.post) {
        WLOGERROR("broadcast data to atgateway 0x%llx failed when malloc post", static_cast<unsigned long long>(bus_id));
        return hello::err::EN_SYS_MALLOC;
    }
    msg.body.post->session_ids.assign(session_ids.begin(), session_ids.end());

    std::stringstream ss;
    msgpack::pack(ss, msg);
    std::string packed_buffer;
    ss.str().swap(packed_buffer);

    int ret = owner->get_bus_node()->send_data(bus_id, ::atframe::component::service_type::EN_ATST_GATEWAY, packed_buffer.data(), packed_buffer.size());
    if (ret < 0) {
        WLOGERROR("broadcast data to atgateway 0x%llx failed, res: %d", static_cast<unsigned long long>(bus_id), ret);
    }

    return ret;
}