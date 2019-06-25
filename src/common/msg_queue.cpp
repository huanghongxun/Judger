#include "common/msg_queue.hpp"
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sstream>
#include <system_error>
#include <vector>
#include "common/stl_utils.hpp"

namespace judge::message {
using namespace std;

queue::queue() {
    id = msgget(IPC_PRIVATE, IPC_EXCL | IPC_CREAT | 0600);
    if (id < 0) {
        throw std::system_error();
    }
}

queue::~queue() {
    if (msgctl(id, IPC_RMID, nullptr) < 0)
        throw std::system_error();
}

void queue::ack(envelope env) const {
    if (!(env.delivery_info & 0xFFFFFFFF00000000L)) {
        throw runtime_error("this envelope cannot be acked");
    }

    if (msgsnd(id, &env.delivery_info, sizeof(env.delivery_info), 0) < 0) {
        throw std::system_error();
    }
}

envelope queue::send_message(const void *data, size_t len, int type, int flag) {
    stringstream ss;
    ss << long(type);

    long actual_type = type;
    if (flag & ACK) {
        lock_guard<mutex> guard(packet_id_mutex);
        actual_type = long(++packet_id) << 32 | (type & 0xFFFFFFFF);
        ss << actual_type;
    }
    ss.write((const char *)data, len);
    if (msgsnd(id, data, len, flag) < 0) {
        if (errno == EAGAIN)
            return {false, 0};
        else
            throw std::system_error();
    }
    return {true, actual_type};
}

envelope queue::recv_message(void *data, size_t len, int type, int flag) const {
    if (msgrcv(id, data, len, type, flag) < 0) {
        if (errno == ENOMSG)
            return {false, 0};
        else
            throw std::system_error();
    }

    if (flag & ACK) {
        if (len < sizeof(long))
            throw runtime_error("Message cannot be acked");
        long actual_type = *(long *)data;
        memmove(data, (char *)data + sizeof(long), len - sizeof(long));
        return {true, actual_type};
    } else {
        return {true, type};
    }
}

envelope queue::recv_var_message(vector<char> &data, int type, int flag) const {
    while (true) {
        auto flag = msgrcv(id, data.data(), data.size(), type, 0);

        if (flag >= 0) break;
        if (errno == E2BIG) {
            data.resize(data.size() * 2);
        } else if (errno == ENOMSG) {
            return {false, 0};
        } else {
            throw std::system_error();
        }
    }
    if (flag & ACK) {
        if (data.size() < sizeof(long))
            throw runtime_error("Message cannot be acked");
        long actual_type = *(long *)&data[0];
        data.erase(data.begin(), data.begin() + sizeof(long));
        return {true, actual_type};
    } else {
        return {true, type};
    }
}

envelope queue::send_string(int type, string &&data, int flag) {
    // 发送格式：
    // 数据的头 4 个字节为 type
    // 然后为字符串本身，最后手动添加 \0 结尾
    stringstream ss;
    ss << data << bits(0);
    string str = ss.str();
    return send_message(str.c_str(), str.size(), type, flag);
}

envelope queue::recv_string(int type, string &data, int flag) const {
    vector<char> buf;
    buf.resize(sizeof(size_t));
    envelope env = recv_var_message(buf, type, flag);
    if (!env.success) return env;
    size_t i = sizeof(size_t);
    for (; i < buf.size(); ++i)
        if (buf[i] == 0) break;
    if (i >= buf.size())
        throw runtime_error("Message malformed");
    data = string(buf.begin() + sizeof(size_t), buf.begin() + i);
    return env;
}

envelope queue::recv_json(int type, json &data, int flag) const {
    string str;
    envelope env = recv_string(type, str, flag);
    data = json::parse(str);
    return env;
}

}  // namespace judge::message
