//
// Created by owt50 on 2016/9/27.
//

#include <log/log_wrapper.h>

#include <atframe/atapp.h>
#include <libatbus.h>
#include <proto_base.h>


#include <config/atframe_service_types.h>
#include <config/extern_service_types.h>


#include "ss_msg_dispatcher.h"

#include <protocol/pbdesc/svr.const.err.pb.h>
#include <protocol/pbdesc/svr.container.pb.h>
#include <utility/protobuf_mini_dumper.h>


ss_msg_dispatcher::ss_msg_dispatcher() {}
ss_msg_dispatcher::~ss_msg_dispatcher() {}

const char *ss_msg_dispatcher::name() const { return "ss_msg_dispatcher"; }

int32_t ss_msg_dispatcher::init() { return 0; }

int32_t ss_msg_dispatcher::unpack_msg(msg_ptr_t msg_container, const void *msg_buf, size_t msg_size) {
    if (NULL == msg_container) {
        WLOGERROR("parameter error");
        return hello::err::EN_SYS_PARAM;
    }

    uint64_t src_bus_id = msg_container->src_server().bus_id();
    if (false == msg_container->ParseFromArray(msg_buf, static_cast<int>(msg_size))) {
        WLOGERROR("unpack msg failed\n%s", msg_container->InitializationErrorString().c_str());
        return hello::err::EN_SYS_UNPACK;
    }
    // reset src bus id, because it will be rewritten by ParseFromArray
    msg_container->mutable_src_server()->set_bus_id(src_bus_id);

    return hello::err::EN_SUCCESS;
}

uint64_t ss_msg_dispatcher::pick_msg_task(const msg_ptr_t msg_container) {
    if (NULL == msg_container || !msg_container->has_src_server()) {
        return 0;
    }

    return msg_container->src_server().dst_task_id();
}

const std::string &ss_msg_dispatcher::pick_msg_name(const msg_ptr_t msg_container) {
    if (NULL == msg_container) {
        return get_empty_string();
    }

    if (false == msg_container->has_ssmsg()) {
        return get_empty_string();
    }

    if (false == msg_container->ssmsg().has_body()) {
        return get_empty_string();
    }

    //    std::vector<const google::protobuf::FieldDescriptor *> output;
    //    msg_container->ssmsg().body().GetReflection()->ListFields(msg_container->ssmsg().body(), &output);
    //    if (output.empty()) {
    //        return get_empty_string();
    //    }
    //
    //    if (output.size() > 1) {
    //        WLOGERROR("there is more than one body");
    //        for (size_t i = 0; i < output.size(); ++i) {
    //            WLOGERROR("body[%d]=%s", static_cast<int>(i), output[i]->name().c_str());
    //        }
    //    }
    //
    //    return output[0]->name();
    const google::protobuf::FieldDescriptor *fd =
        msg_container->ssmsg().body().GetReflection()->GetOneofFieldDescriptor(msg_container->ssmsg().body(), get_body_oneof_desc());

    if (NULL == fd) {
        return get_empty_string();
    }

    return fd->name();
}

ss_msg_dispatcher::msg_type_t ss_msg_dispatcher::pick_msg_type_id(const msg_ptr_t msg_container) {
    if (NULL == msg_container) {
        return 0;
    }

    if (false == msg_container->has_ssmsg()) {
        return 0;
    }

    if (false == msg_container->ssmsg().has_body()) {
        return 0;
    }

    //    std::vector<const google::protobuf::FieldDescriptor *> output;
    //    msg_container->ssmsg().body().GetReflection()->ListFields(msg_container->ssmsg().body(), &output);
    //    if (output.empty()) {
    //        return 0;
    //    }
    //
    //    if (output.size() > 1) {
    //        WLOGERROR("there is more than one body");
    //        for (size_t i = 0; i < output.size(); ++i) {
    //            WLOGERROR("body[%d]=%s", static_cast<int>(i), output[i]->name().c_str());
    //        }
    //    }
    //
    //    return static_cast<msg_type_t>(output[0]->number());
    return static_cast<msg_type_t>(msg_container->ssmsg().body().body_oneof_case());
}

ss_msg_dispatcher::msg_type_t ss_msg_dispatcher::msg_name_to_type_id(const std::string &msg_name) {
    const google::protobuf::FieldDescriptor *desc = hello::SSMsgBody::descriptor()->FindFieldByName(msg_name);
    if (NULL == desc) {
        return 0;
    }

    return static_cast<msg_type_t>(desc->number());
}

int32_t ss_msg_dispatcher::send_to_proc(uint64_t bus_id, msg_ptr_t msg) {
    if (NULL == msg) {
        WLOGERROR("parameter error");
        return hello::err::EN_SYS_PARAM;
    }

    size_t msg_buf_len = static_cast<size_t>(msg->ByteSize());
    size_t tls_buf_len = atframe::gateway::proto_base::get_tls_length(atframe::gateway::proto_base::tls_buffer_t::EN_TBT_CUSTOM);
    if (msg_buf_len > tls_buf_len) {
        WLOGERROR("send to proc [0x%llx] failed: require %llu, only have %llu", static_cast<unsigned long long>(bus_id),
                  static_cast<unsigned long long>(msg_buf_len), static_cast<unsigned long long>(tls_buf_len));
        return hello::err::EN_SYS_BUFF_EXTEND;
    }

    ::google::protobuf::uint8 *buf_start =
        reinterpret_cast< ::google::protobuf::uint8 *>(atframe::gateway::proto_base::get_tls_buffer(atframe::gateway::proto_base::tls_buffer_t::EN_TBT_CUSTOM));
    msg->SerializeWithCachedSizesToArray(buf_start);
    WLOGDEBUG("send msg to proc [0x%llx] %llu bytes\n%s", static_cast<unsigned long long>(bus_id), static_cast<unsigned long long>(msg_buf_len),
              protobuf_mini_dumper_get_readable(*msg));

    return send_to_proc(bus_id, buf_start, msg_buf_len);
}

int32_t ss_msg_dispatcher::send_to_proc(uint64_t bus_id, const void *msg_buf, size_t msg_len) {
    atapp::app *owner = get_app();
    if (NULL == owner) {
        WLOGERROR("module not attached to a atapp");
        return hello::err::EN_SYS_INIT;
    }

    if (!owner->get_bus_node()) {
        WLOGERROR("owner app has no valid bus node");
        return hello::err::EN_SYS_INIT;
    }

    int res = owner->get_bus_node()->send_data(bus_id, atframe::component::message_type::EN_ATST_SS_MSG, msg_buf, msg_len, false);

    if (res < 0) {
        WLOGERROR("send msg to proc [0x%llx] %llu bytes failed, res: %d", static_cast<unsigned long long>(bus_id), static_cast<unsigned long long>(msg_len),
                  res);
    } else {
        WLOGDEBUG("send msg to proc [0x%llx] %llu bytes success", static_cast<unsigned long long>(bus_id), static_cast<unsigned long long>(msg_len));
    }

    return res;
}

int32_t ss_msg_dispatcher::dispatch(const atbus::protocol::msg &msg, const void *buffer, size_t len) {
    if (::atframe::component::message_type::EN_ATST_SS_MSG != msg.head.type) {
        WLOGERROR("message type %d invalid", msg.head.type);
        return hello::err::EN_SYS_PARAM;
    }

    if (NULL == msg.body.forward || 0 == msg.head.src_bus_id) {
        WLOGERROR("receive a message from unknown source");
        return hello::err::EN_SYS_PARAM;
    }

    hello::message_container ss_msg;
    ss_msg.mutable_src_server()->set_bus_id(msg.body.forward->from);

    int32_t ret = on_recv_msg(&ss_msg, buffer, len);
    if (ret < 0) {
        WLOGERROR("dispatch ss message from 0x%llx failed, res: %d", static_cast<unsigned long long>(msg.body.forward->from), ret);
    }

    return ret;
}

int32_t ss_msg_dispatcher::notify_send_failed(const atbus::protocol::msg &msg, const void *buffer, size_t len) {
    if (::atframe::component::message_type::EN_ATST_SS_MSG != msg.head.type) {
        WLOGERROR("message type %d invalid", msg.head.type);
        return hello::err::EN_SYS_PARAM;
    }

    if (NULL == msg.body.forward) {
        WLOGERROR("send a message from unknown source");
        return hello::err::EN_SYS_PARAM;
    }

    hello::message_container ss_msg;
    ss_msg.mutable_src_server()->set_bus_id(msg.body.forward->from);

    int32_t ret = on_send_msg_failed(&ss_msg, buffer, len, msg.head.ret);
    if (ret < 0) {
        WLOGERROR("dispatch ss message from 0x%llx failed, res: %d", static_cast<unsigned long long>(msg.body.forward->from), ret);
    }

    return ret;
}

const google::protobuf::OneofDescriptor *ss_msg_dispatcher::get_body_oneof_desc() const {
    static const google::protobuf::OneofDescriptor *ret = NULL;
    if (NULL != ret) {
        return ret;
    }

    if (hello::SSMsgBody::descriptor()->oneof_decl_count() > 0) {
        return hello::SSMsgBody::descriptor()->oneof_decl(0);
    }
    //    ret = hello::SSMsgBody::descriptor()->FindOneofByName("body_oneof");
    //    if (NULL == ret) {
    //        WLOGERROR("find oneof descriptor \"body_oneof\" in hello::SSMsgBody failed");
    //    }
    //
    //    return ret;
    return NULL;
}