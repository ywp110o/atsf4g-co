#include "libatgw_proto_inner_v1.h"

namespace atframe {
    namespace gateway {
        libatgw_proto_inner_v1::~libatgw_proto_inner_v1() {}

        void libatgw_proto_inner_v1::alloc_recv_buffer(size_t suggested_size, char *&out_buf, size_t &out_len) {
            io_stream_flag_guard flag_guard(conn_raw_ptr->channel->flags, io_stream_channel::EN_CF_IN_CALLBACK);

            // 如果正处于关闭阶段，忽略所有数据
            if (io_stream_connection::EN_ST_CONNECTED != conn_raw_ptr->status) {
                buf->base = NULL;
                buf->len = 0;
                uv_read_stop(conn_raw_ptr->handle.get());
            }

            void *data = NULL;
            size_t sread = 0, swrite = 0;
            conn_raw_ptr->read_buffers.back(data, sread, swrite);

            // 正在读取vint时，指定缓冲区为head内存块
            if (NULL == data || 0 == swrite) {
                buf->len = sizeof(conn_raw_ptr->read_head.buffer) - conn_raw_ptr->read_head.len;

                if (0 == buf->len) {
                    // 理论上这里不会走到，因为如果必然会先收取一次header的大小，这时候已经可以解出msg的大小
                    // 如果msg超过限制大小并低于缓冲区大小，则会发出大小错误回调并会减少header的占用量，
                    // 那么下一次这个回调函数调用时buf->len必然大于0
                    // 如果msg超过缓冲区大小，则会出错回调并立即断开连接,不会再有下一次调用
                    buf->base = NULL;
                } else {
                    buf->base = &conn_raw_ptr->read_head.buffer[conn_raw_ptr->read_head.len];
                }
                return;
            }

            // 否则指定为大内存块缓冲区
            buf->base = reinterpret_cast<char *>(data);
            buf->len = swrite;
        }

        void libatgw_proto_inner_v1::read(int ssz, const char *buff, size_t len) {
            io_stream_flag_guard flag_guard(channel->flags, io_stream_channel::EN_CF_IN_CALLBACK);

            // TODO alloc call read API of session proto
        }

        int libatgw_proto_inner_v1::write(const void *buffer, size_t len) = 0;
        int libatgw_proto_inner_v1::write_done(int status);
        int libatgw_proto_inner_v1::close(int reason);
        bool libatgw_proto_inner_v1::check_reconnect(proto_base *other);
    }
}