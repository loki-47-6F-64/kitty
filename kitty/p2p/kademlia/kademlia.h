//
// Created by loki on 9-4-19.
//

#ifndef T_MAN_KADEMLIA_H
#define T_MAN_KADEMLIA_H

#include <chrono>
#include <kitty/p2p/uuid.h>
#include <kitty/server/server.h>
#include <kitty/util/sync.h>
#include <kitty/util/utility.h>


namespace server {

template<class T, class... RecvArgs>
class MailBox {
public:
  using code_t   = T;
  using letter_t = std::pair<std::function<void(RecvArgs...)>, util::TaskPool::task_id_t>;

  template<class ...Args>
  void recv(util::TaskPool &task_pool, const code_t &key, Args&&... args) {
    static_assert(std::is_invocable_v<std::function<void(RecvArgs...)>, decltype(args)...>, "the receive function must be invocable with given arguments");

    letter_t letter;

    {
      auto lock = _letterbox.lock();
      auto it = _letterbox->find(key);

      if(it == std::end(_letterbox.raw)) {
        return;
      }

      letter = std::move(it->second);
    }

    TUPLE_2D_REF(func, task_id, letter);
    bool canceled = task_pool.cancel(task_id);
    if(canceled) {
      func(args...);
    }
  }

  template<class TaskPool, class F1, class F2, class Rep, class Period, class ...Args>
  void pending(
    TaskPool &task_pool,
    F1 &&func,
    F2 &&func_to, const std::chrono::duration<Rep, Period> &timeout,
    const code_t &code,
    Args &&... args) {
    static_assert(std::is_base_of_v<util::TaskPool, TaskPool>, "template parameter TaskPool should be a base class of util::TaskPool");

    auto shared_args = std::make_shared<std::tuple<std::decay_t<Args>...>>(std::forward<Args>(args)...);

    auto on_recv = [shared_args, func = std::forward<F1>(func)](RecvArgs... recv_args) mutable {
      auto on_recv_helper = [&](auto&&... tuple_args) {
        std::invoke(func, std::forward<Args>(tuple_args)..., recv_args...);
      };

      std::apply(on_recv_helper, std::move(*shared_args));
    };

    auto on_timeout = [this, code, shared_args, func = std::forward<F2>(func_to)]() mutable {
      {
        auto lock = _letterbox.lock();

        _letterbox->erase(code);
      }

      auto on_timeout_helper = [&](auto&&... tuple_args) {
        std::invoke(func, std::forward<Args>(tuple_args)...);
      };

      std::apply(on_timeout_helper, std::move(*shared_args));
    };

    auto task_id = task_pool.pushDelayed(std::move(on_timeout), timeout).task_id;

    auto lock = _letterbox.lock();
    _letterbox->emplace(code, letter_t { std::move(on_recv), task_id });
  }

private:
  util::Sync<std::unordered_map<code_t, letter_t, util::hash<code_t>>> _letterbox;
};

}

namespace p2p {

constexpr auto __max_buckets { sizeof(uuid_t) * 8 };

struct node_t {
  uuid_t uuid;
  file::ip_addr_buf_t addr;
};

class Kademlia {
public:
  using data_alarm_t = util::alarm_shared<util::Either<err::code_t, std::vector<std::uint8_t>>>;
  using peer_alarm_t = util::alarm_shared<util::Either<err::code_t, node_t>>;

  using msg_id_t = uuid_t;
  using poll_t = file::poll_t<file::demultiplex, Kademlia*>;

  using shared_sock_t = std::shared_ptr<util::Sync<file::demultiplex, 2>>;

  Kademlia(
    uuid_t uuid,
    std::size_t max_bucket_size,
    std::size_t parallel_lookups,
    std::chrono::seconds refresh,
    std::chrono::seconds ttl,
    std::chrono::milliseconds response_timeout
  ) :
  uuid { uuid },
  max_bucket_size { max_bucket_size },
  parallel_lookups { parallel_lookups },
  refresh { refresh },
  ttl { ttl },
  response_timeout { response_timeout },
  poll { &Kademlia::_on_read, [](auto&, auto&) {} } {};

public:
  /**
   * @param key the unique identifier of the value
   * @param data the data to be indexed
   */
  util::alarm_shared<err::code_t> publish(const uuid_t &key, const std::string_view &data);

  /**
   * @param key the unique identifier of the published value
   */
  data_alarm_t get_value(const uuid_t &key);

  /**
   * @param key the unique identifier of the published value
   */
  peer_alarm_t get_peer(const uuid_t &key);

  std::pair<file::ip_addr_t, uuid_t> addr();

  int start(std::vector<node_t> &&stable_nodes, std::uint16_t port, util::alarm_shared<err::code_t> alarm);
  void stop() {
    server.stop();
  }

  bool is_running() const {
    return server.isRunning();
  }

  // TODO: create mechanism to unregister from the network
private:
  static void _on_read(file::demultiplex &, Kademlia *kad);

  void _update_buckets(node_t &peer);
  void _erase_node(const node_t &peer);
  void _join_network(std::size_t x, std::vector<node_t> &&stable_nodes, util::alarm_shared<err::code_t> alarm);

  void _poll();

  void _on_pending_timeout(uuid_t bucket_key, const node_t &old, node_t &_new);
  void _on_pending_response(uuid_t bucket_key, const node_t &old, node_t &_new, const shared_sock_t&);

  void _on_lookup_timeout(uuid_t peer_key, peer_alarm_t &alarm);
  void _on_lookup_response(uuid_t peer_key, peer_alarm_t &alarm, shared_sock_t &sock_in);

  uuid_t uuid;

  std::size_t max_bucket_size;
  std::size_t parallel_lookups;

  std::chrono::seconds refresh;
  std::chrono::seconds ttl;

  std::chrono::milliseconds response_timeout;

  server::MailBox<uuid_t, shared_sock_t> mailbox;
public:
  server::udp server;

  poll_t poll;

  /**
   * routing table
   */
  util::Sync<std::map<uuid_t, std::vector<std::pair<node_t, bool>>>> buckets;

  /**
   * store the sockets
   */
  util::Sync<std::unordered_map<server::ip_addr_key, shared_sock_t>> sockets;

  std::vector<node_t> pending;
};

}
#endif //T_MAN_KADEMLIA_H
