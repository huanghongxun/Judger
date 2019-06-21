#pragma once

#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace judge::message {
using namespace std;
using namespace nlohmann;

/**
 * @brief 发送的消息信封
 */
struct envelope {
    /**
     * @brief 表示是否有消息收到
     * 如果在 flag 中未设置 NO_WAIT，则本项总是为 true，否则：
     * 对于消息发送，表示是否发送成功；
     * 对于消息接收，表示是否存在消息接收到了，false 表示消息队列目前没有消息。
     */
    const bool success;
    const long delivery_info;
};

/**
 * @brief 消息队列
 * 使用方法：在 fork 前调用 message_queue 的构造函数创建一个消息队列，
 * 然后调用 fork 创建子进程，通过同一个 message_queue 对象来进行通信。
 */
struct queue {
    queue();
    ~queue();

    // clang-format off
    static constexpr int NO_WAIT    = 0004000;
    static constexpr int NO_ERROR   = 0010000;
    static constexpr int EXCEPT     = 0020000;
    static constexpr int COPY       = 0040000;
    static constexpr int ACK        = 0100000;
    static constexpr int STAT       = 11;
    static constexpr int INFO       = 12;
    // clang-format on

    /**
     * @brief 发送一条消息到消息队列中。
     * 由于不会类型检查，请确保 send 和 recv 在同 type 下使用的类型完全一致
     * @param type 消息的类型，提供给使用者自行区分消息的 topic
     * @param data 消息的内容，消息将通过 IPC 来传输
     * @param flag
     *             MSG_ACK 是否需要接收方发送 ack 确认对方收到可用于判断对方是否完成了消息的处理
     *             IPC_NOWAIT 如果不存在消息，则立即返回
     * @param <T> 消息内容的类型，需要与同 type 的 recv 函数的 <T> 一致，且支持与 json 的转换
     */
    template <typename T>
    envelope send_as_json(int type, const T &data, int flag = 0) {
        json j = data;  // convert to json
        return send_string(type, j, flag);
    }

    template <typename T>
    envelope send_as_json(const T &data, int flag = 0) {
        return send_as_json(remove_reference_t<T>::ID, data, flag);
    }

    template <typename T>
    envelope send_as_pod(int type, const T &data, int flag = 0) {
        static_assert(is_trivially_copyable_v<remove_reference_t<T>>, "T must be trivially copyable");
        return send_message(&data, sizeof(remove_reference_t<T>), type, flag);
    }

    template <typename T>
    envelope send_as_pod(const T &data, int flag = 0) {
        return send_as_pod(remove_reference_t<T>::ID, data, flag);
    }

    envelope send_string(int type, string &&data, int flag);

    /**
     * @brief 从消息队列中接收一条消息。
     * 由于不会类型检查，请确保 send 和 recv 在同 type 下使用的类型完全一致
     * @param type 消息的类型，提供给使用者自行区分消息的 topic
     * @param data 消息的内容，消息将通过 IPC 来传输，注意不可以使用任何持有堆内存的类型，否则传输时会失败
     * @param flag
     *             MSG_ACK 是否需要接收方发送 ack 确认对方收到可用于判断对方是否完成了消息的处理
     *             IPC_NOWAIT 如果不存在消息，则立即返回
     * @param <T> 消息内容的类型，需要与同 type 的 send 函数的 <T> 一致，且支持与 json 的转换
     * @return 消息的 id，如果 ack = true，那么你可以使用这个返回值来发送 ack 包
     */
    template <typename T>
    envelope recv_from_json(int type, T &data, int flag = 0) const {
        json j;
        auto env = recv_json(type, j, flag);
        j.get_to(data);
        return env;
    }

    template <typename T>
    envelope recv_from_json(T &data, int flag = 0) const {
        return recv(T::ID, data, flag);
    }

    template <typename T>
    envelope recv_from_pod(int type, T &data, int flag = 0) const {
        static_assert(is_trivially_copyable_v<T>, "T must be pod");
        return recv_message(&data, sizeof(T), type, flag);
    }

    template <typename T>
    envelope recv_from_pod(T &data, int flag = 0) const {
        return recv_from_pod(T::ID, data, flag);
    }

    envelope recv_string(int type, string &data, int flag) const;

    envelope recv_json(int type, json &data, int flag) const;

    void ack(envelope env) const;

private:
    /**
     * @brief 消息队列的 id
     * 通过 msgget 获得
     */
    int id;

    int packet_id = 0;
    mutex packet_id_mutex;

    /**
     * @brief 发送消息
     * 此函数为 msgsnd 的包装
     */
    envelope send_message(const void *data, size_t len, int type, int flag);

    /**
     * @brief 接收消息
     * 此函数为 msgrcv 的包装
     */
    envelope recv_message(void *data, size_t len, int type, int flag) const;

    /**
     * @brief 接收变长消息
     * 此函数为 msgrcv 的封装
     */
    envelope recv_var_message(vector<char> &data, int type, int flag) const;
};

}  // namespace judge::message
