#pragma once
#include <type_traits>
#include <orion/noncopyable.hpp>
#include <orion/bosen/host_info.hpp>
#include <orion/bosen/conn.hpp>

namespace orion {
namespace bosen {

namespace message {

enum class Type {
  kMasterMsg = 0,
    kDriverMsg = 1,
    kExecutorConnectToPeers = 2,
    kExecutorConnectToPeersAck = 3,
    kExecutorIdentity = 4,
    kExecutorReady = 5,
    kExecutorStop = 10
};

class Helper;
class DefaultMsgCreator;

struct Header {
  Type type;
  uint64_t seq, ack;
 private:
  Header() = default;
  friend class Helper;
 public:
  Header (Type _type):
      type(_type) { }
};

static_assert(std::is_pod<Header>::value, "Header must be POD!");

struct ExecutorConnectToPeers {
 public:
  size_t num_executors;
 private:
  ExecutorConnectToPeers() = default;
  friend class DefaultMsgCreator;
 public:
  void Init(size_t _num_executors) {
    num_executors = _num_executors;
  }
  static constexpr Type get_type() { return Type::kExecutorConnectToPeers; }
};

static_assert(std::is_pod<ExecutorConnectToPeers>::value,
              "ExecutorConnectToPeers must be POD!");

struct ExecutorConnectToPeersAck {
 private:
  ExecutorConnectToPeersAck() = default;
  friend class DefaultMsgCreator;
 public:
  void Init() { }
  static constexpr Type get_type() { return Type::kExecutorConnectToPeersAck; }
};

static_assert(std::is_pod<ExecutorConnectToPeersAck>::value,
                "ExecutorConnectToPeersAck must be POD!");

struct ExecutorStop {
 private:
  ExecutorStop() = default;
  friend class DefaultMsgCreator;
 public:
  void Init() { }
  static constexpr Type get_type() { return Type::kExecutorStop; }
};
static_assert(std::is_pod<ExecutorStop>::value,
              "ExecutorStop must be POD!");

struct ExecutorIdentity {
  int32_t executor_id;
  HostInfo host_info;
 private:
  ExecutorIdentity() = default;
  friend class DefaultMsgCreator;
 public:
  void Init(int32_t _executor_id, HostInfo _host_info) {
    executor_id = _executor_id;
    host_info = _host_info;
  }
  static constexpr Type get_type() { return Type::kExecutorIdentity; }
};

static_assert(std::is_pod<ExecutorIdentity>::value, "ExecutorIdentity must be POD!");

enum class DriverMsgType {
  kStop = 0,
    kExecuteOnAny = 1
};

struct DriverMsg {
  DriverMsgType driver_msg_type;
 private:
  DriverMsg() = default;
  friend class DefaultMsgCreator;
 public:
  void Init() { }
  static constexpr Type get_type() { return Type::kDriverMsg; }
};

static_assert(std::is_pod<DriverMsg>::value,
              "DriverMsg must be POD!");

class DefaultMsgCreator {
 public:
  template<typename Msg, typename... Args>
  static Msg* CreateMsg(uint8_t* mem, Args... args) {
    auto msg = new (mem) Msg();
    msg->Init(args...);
    return msg;
  }
};

class Helper {
 public:
  template<typename Msg,
           typename MsgCreator = DefaultMsgCreator,
           typename... Args>
  static Msg* CreateMsg(conn::SendBuffer *send_buff,
                        Args... args) {
    auto header = new (send_buff->get_payload_mem()) Header();
    header->type = Msg::get_type();
    Msg* msg = MsgCreator::template CreateMsg<Msg, Args...>(
        send_buff->get_payload_mem() + sizeof(Header), args...);
    CHECK_GE(send_buff->get_payload_capacity(), sizeof(Header) + sizeof(Msg));
    send_buff->set_payload_size(sizeof(Header) + sizeof(Msg));
    return msg;
  }

  static Type get_type(conn::RecvBuffer& recv_buff) {
    return reinterpret_cast<const Header*>(recv_buff.get_payload_mem())->type;
  }

  template<typename Msg>
  static Msg* get_msg(conn::RecvBuffer &recv_buff) {
    return reinterpret_cast<Msg*>(recv_buff.get_payload_mem() + sizeof(Header));
  }

  template<typename Msg>
  static uint8_t* get_remaining_mem(conn::RecvBuffer &recv_buff) {
    return recv_buff.get_payload_mem() + sizeof(Header) + sizeof(Msg);
  }

  template<typename Msg>
  static size_t get_remaining_size(const conn::RecvBuffer &recv_buff) {
    return recv_buff.get_payload_capacity() - sizeof(Header) - sizeof(Msg);
  }
};

} // end of namespace message

} // end of namespace bosen
}