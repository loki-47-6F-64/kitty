//
// Created by loki on 9-4-19.
//

#include <cstring>
#include <kitty/server/proxy.h>
#include <kitty/util/set.h>
#include "kademlia.h"
#include <kitty/p2p/kademlia/data_types.h>

namespace p2p {
using namespace std::literals;

template<class T, class X>
const T* map_find(const std::unordered_map<X, T> &map, const X& key) {
  auto it = map.find(key);

  if(it == std::end(map)) {
    return nullptr;
  }

  return &it->second;
}

template<class T, class X>
T* map_find(std::unordered_map<X, T> &map, const X& key) {
  auto it = map.find(key);

  if(it == std::end(map)) {
    return nullptr;
  }

  return &it->second;
}

template<class T, class X>
const T* map_find(const std::map<X, T> &map, const X& key) {
  auto it = map.find(key);

  if(it == std::end(map)) {
    return nullptr;
  }

  return &it->second;
}

template<class T, class X>
T* map_find(std::map<X, T> &map, const X& key) {
  auto it = map.find(key);

  if(it == std::end(map)) {
    return nullptr;
  }

  return &it->second;
}

/**
 * Copied from https://www.geeksforgeeks.org/find-significant-set-bit-number/
 * @return the most significant bit
 */
constexpr std::uint8_t significant_bit(std::uint8_t n)
{
  constexpr auto most_significant_bit = 1u << 7u;
  if(n & most_significant_bit) {
    return most_significant_bit;
  }

  // Below steps set bits after
  // MSB (including MSB)

  // Suppose n is 273 (binary
  // is 100010001). It does following
  // 100010001 | 010001000 = 110011001
  n |= n >> 1;

  // This makes sure 4 bits
  // (From MSB and including MSB)
  // are set. It does following
  // 110011001 | 001100110 = 111111111
  n |= n >> 2;
  n |= n >> 4;

  // Increment n by 1 so that
  // there is only one set bit
  // which is just before original
  // MSB. n now becomes 1000000000
  ++n;

  // Return original MSB after shifting.
  // n now becomes 100000000
  return (n >> 1);
}

template<class T>
constexpr T significant_bit(T val) {
  constexpr auto size = sizeof(T);

  auto val_p = reinterpret_cast<uint8_t*>(&val);
  auto pos = val_p;
  while((pos - val_p) != size) {
    auto bit = significant_bit(*pos);
    if(bit) {
      std::memset(val_p, 0, size);
      *pos = bit;

      return val;
    }

    ++pos;
  }

  // no bit set
  std::memset(val_p, 0, size);
  return val;
}

uuid_t distance(const uuid_t &l, const uuid_t &r) {
  uuid_t res;

  res.b64[0] = l.b64[0] ^ r.b64[0];
  res.b64[1] = l.b64[1] ^ r.b64[1];

  return res;
}

constexpr uuid_t to_bucket_key(const uuid_t &uuid) {
  return significant_bit(uuid);
}

enum class __kademlia_e : std::uint8_t {
  PING,
  LOOKUP,
  RESPONSE
};

std::string_view from_type(__kademlia_e type) {
  switch(type) {
    case __kademlia_e::PING:
      return "PING"sv;
    case __kademlia_e::LOOKUP:
      return "LOOKUP"sv;
    case __kademlia_e::RESPONSE:
      return "RESPONSE"sv;
  }

  return {};
}

std::shared_ptr<util::Sync<file::demultiplex, 2>> __get_socket(
  util::Sync<std::unordered_map<server::ip_addr_key, Kademlia::shared_sock_t>> &sockets,
  const file::ip_addr_t &addr) {
  auto lock = sockets.lock();

  auto it = sockets->find(server::ip_addr_key { addr });

  if(it == std::end(sockets.raw)) {
    err::code = err::OUT_OF_BOUNDS;
    return nullptr;
  }

  return it->second;
}

template<class ...Args>
int __push(
  Kademlia::shared_sock_t &socket,
  Args&&... args)
{
  auto lock = socket->lock<0>();

  return server::proxy::push(socket->raw, std::forward<Args>(args)...);
}

template<class ...Args>
int __load(Kademlia::shared_sock_t &socket, Args&... args) {
  auto lock = socket->lock<1>();

  return server::proxy::load(socket->raw, args...);
}

void __insert_closest_node(std::vector<node_t> &closest_nodes, const uuid_t &key, const node_t &node, std::size_t max_nodes) {
  auto pos = std::find_if(std::begin(closest_nodes), std::end(closest_nodes), [&](auto &l_node) {
    return distance(node.uuid, key) < distance(l_node.uuid, key);
  });

  if(pos != std::end(closest_nodes)) {
    if(closest_nodes.size() == max_nodes) {
      closest_nodes.pop_back();
    }

    closest_nodes.insert(pos, node);
  }
}

std::vector<node_t> __closest_nodes(const uuid_t &key, const std::map<uuid_t, std::vector<std::pair<node_t, bool>>> &vec_nodes, std::size_t max_nodes) {
  std::vector<node_t> closest_nodes;
  closest_nodes.reserve(max_nodes);

  for(auto &[_,nodes] : vec_nodes) {
    for(auto &[node,_] : nodes) {
      __insert_closest_node(closest_nodes, key, node, max_nodes);
    }
  }

  return closest_nodes;
}

std::vector<node_t> __closest_nodes(const uuid_t &key, const std::vector<node_t> &vec_nodes, std::size_t max_nodes) {
  std::vector<node_t> closest_nodes;
  closest_nodes.reserve(max_nodes);

  for(auto &node : vec_nodes) {
    __insert_closest_node(closest_nodes, key, node, max_nodes);
  }

  return closest_nodes;
}

void Kademlia::_on_lookup_timeout(p2p::uuid_t peer_key, peer_alarm_t &alarm) {
  alarm->ring(err::TIMEOUT);
}

void Kademlia::_on_lookup_response(uuid_t peer_key, peer_alarm_t &alarm, Kademlia::shared_sock_t &sock_in) {
  std::vector<data::node_t> nodes_packed;

  if(__load(sock_in, nodes_packed)) {
    alarm->ring(err::code);

    return;
  }

  auto pos = std::find_if(std::begin(nodes_packed), std::end(nodes_packed), [&peer_key](const data::node_t &node) {
    return std::get<1>(node) == peer_key;
  });

  if(pos != std::end(nodes_packed)) {
    alarm->ring(data::unpack(*pos));

    return;
  }

  auto nodes = __closest_nodes(peer_key, util::map(nodes_packed, &data::unpack), parallel_lookups);


  int ret = -1;

  auto msg_id = msg_id_t::generate();
  for(auto &node : nodes) {
    auto sock_out = __get_socket(sockets, node.addr);

    if(!__push(sock_out, msg_id, uuid, __kademlia_e::LOOKUP, peer_key)) {
      ret = 0;
    }
    else {
      auto ip = file::peername(sock_out->raw);

      print(error, "Couldn't push to ", ip.ip, ':', ip.port, ": ", err::current());
    }
  }

  if(ret) {
    alarm->ring(err::INPUT_OUTPUT);

    return;
  }

  mailbox.pending(server.tasks(),
    &Kademlia::_on_lookup_response, &Kademlia::_on_lookup_timeout, response_timeout,
    msg_id,
    this, peer_key, alarm);
}

Kademlia::peer_alarm_t Kademlia::get_peer(const uuid_t &key) {
  peer_alarm_t alarm = std::make_shared<peer_alarm_t::element_type>();

  std::vector<node_t> nodes;
  {
    auto lock = buckets.lock();

    nodes = __closest_nodes(key, buckets.raw, 3);
  }

  auto msg_id = msg_id_t::generate();

  // if all pushes fail --> ret is still -1
  int ret = -1;
  for(auto &node : nodes) {
    auto sock = __get_socket(sockets, node.addr);

    if(!__push(sock, msg_id, uuid, __kademlia_e::LOOKUP, key)) {
      ret = 0;
    }
    else {
      auto ip = file::peername(sock->raw);

      print(error, "Couldn't push to ", ip.ip, ':', ip.port, ": ", err::current());
    }
  }


  if(ret) {
    alarm->ring(err::code);
  }

  mailbox.pending(server.tasks(),
    &Kademlia::_on_lookup_response, &Kademlia::_on_lookup_timeout, response_timeout,
    msg_id,
    this, key, alarm);

  return alarm;
}

void Kademlia::_join_network(std::size_t x, std::vector<p2p::node_t> &&stable_nodes,
                             util::alarm_shared<err::code_t> alarm) {

  if(x >= stable_nodes.size()) {
    alarm->ring(err::TIMEOUT);

    return;
  }

  auto &node = stable_nodes[x];

  if(x > 0) {
    _erase_node(stable_nodes[x - 1]);
  }

  auto bucket_key = to_bucket_key(node.uuid);
  auto lock = buckets.lock();

  auto bucket = map_find(buckets.raw, bucket_key);
  if(!bucket) {
    bucket = &(buckets->emplace(bucket_key, std::vector<std::pair<node_t, bool>> { }).first->second);
  }

  auto on_response = [](Kademlia *, std::size_t, auto&&, auto &alarm, const auto&) {
    alarm->ring(err::OK);
  };

  auto msg_id = msg_id_t::generate();

  shared_sock_t sock_p;

  {
    auto lock = sockets.lock();

    auto demux = std::make_shared<shared_sock_t::element_type>(3s, server::get_multiplex(server).demux(node.addr).first);
    sock_p = sockets->emplace(node.addr, std::move(demux)).first->second;
  }

  if(__push(sock_p, msg_id, uuid, __kademlia_e::LOOKUP, uuid)) {
    print(error, "Couldn't join network: ", err::current());


    alarm->ring(err::code);
    return;
  }

  poll.read(sock_p->raw, this);

  mailbox.pending(server.tasks(),
    std::move(on_response), &Kademlia::_join_network,
    response_timeout, msg_id,
    this,
    x + 1,
    std::move(stable_nodes),
    alarm);
}

void Kademlia::_on_pending_timeout(p2p::uuid_t bucket_key, const p2p::node_t &old, p2p::node_t &_new) {
  auto lock = buckets.lock();

  auto bucket = map_find(buckets.raw, bucket_key);

  assert(bucket);

  auto pos_bucket = std::find_if(std::begin(*bucket), std::end(*bucket), [&](auto &bucket_node) {
    return old.uuid == bucket_node.first.uuid;
  });

  auto pos_pending = std::find_if(std::begin(pending), std::end(pending), [&](auto &pending_node) {
    return _new.uuid == pending_node.uuid;
  });

  assert(pos_bucket != std::end(*bucket));
  assert(pos_pending != std::end(pending));

  *pos_bucket = std::pair { std::move(*pos_pending), false };

  pending.erase(pos_pending);

  // move new node to most recently seen position
  std::rotate(pos_bucket, pos_bucket +1, std::end(*bucket));
}

void Kademlia::_on_pending_response(p2p::uuid_t bucket_key, const p2p::node_t &old, p2p::node_t &_new, const shared_sock_t&) {
  auto lock = buckets.lock();

  auto pos_pending = std::find_if(std::begin(pending), std::end(pending), [&](auto &pending_node) {
    return _new.uuid == pending_node.uuid;
  });

  pending.erase(pos_pending);
}

int Kademlia::start(std::vector<node_t> &&stable_nodes, std::uint16_t port, util::alarm_shared<err::code_t> alarm) {
  server.tasks().push(
    &Kademlia::_join_network,
    this, 0, std::move(stable_nodes), alarm
  );

  server.tasks().push(&Kademlia::_poll, this);

  return server.start([this](server::udp_client_t &&client) {
    auto ip = file::peername(client.socket);
    auto sock = std::make_shared<shared_sock_t::element_type>(std::move(client.socket));

    auto sock_p = sock.get();
    {
      auto lock = sockets.lock();
      sockets->emplace(std::move(ip), std::move(sock));
    }

    poll.read(sock_p->raw, this);
  }, port);
}

void Kademlia::_on_read(file::demultiplex &sock, Kademlia *kad) {
  msg_id_t msg_id;
  uuid_t peer_uuid;
  __kademlia_e type;

  auto ip = file::peername(sock);
  auto sock_p = __get_socket(kad->sockets, ip);

  if(__load(sock_p, msg_id, peer_uuid, type)) {
    print(error, "Couldn't load data from ", ip.ip, ':', ip.port, ": ", err::current());

    if(err::code == err::LIB_SYS) {
      std::abort();
    }

    sock.seal();
    kad->poll.remove(sock);
    return;
  }

  node_t peer_node { peer_uuid, file::peername(sock) };
  kad->_update_buckets(peer_node);

  print(info, peer_node.addr.ip, ':', peer_node.addr.port, '(', from_type(type), ") [", util::hex(msg_id), "] [", util::hex(peer_uuid), ']');

  switch(type) {
    case __kademlia_e::PING:
      if(__push(sock_p, msg_id, kad->uuid, __kademlia_e::RESPONSE)) {
        print(error, "Couldn't push PONG: ", err::current());
        if(err::code == err::TIMEOUT) {
          // If this ever hits, there are to many messages waiting to be send.

          return;
        }

        if(err::code == err::LIB_SYS) {
          //TODO: iterate over recoverable errors

          std::abort();
        }

        sock.seal();
        kad->poll.remove(sock);
      }
      break;
    case __kademlia_e::LOOKUP: {
      auto lock = kad->buckets.lock();

      uuid_t lookup_peer;
      if(__load(sock_p, lookup_peer)) {
        print(error, "Couldn't load peer to be looked up: ", err::current());

        if(err::code == err::TIMEOUT) {
          return;
        }

        sock.seal();
        kad->poll.remove(sock);
      }

      auto nodes = util::map(__closest_nodes(peer_uuid, kad->buckets.raw, kad->max_bucket_size), [](const node_t &node) {
        return data::pack(node);
      });

      if(__push(sock_p, msg_id, kad->uuid, __kademlia_e::RESPONSE, nodes)) {
        print(error, "Couldn't push nodes: ", err::current());

        if(err::code == err::TIMEOUT) {
          // If this ever hits, there are to many messages waiting to be send.

          return;
        }

        sock.seal();
        kad->poll.remove(sock);

        return;
      }
    }
    case __kademlia_e::RESPONSE:
      kad->mailbox.recv(kad->server.tasks(), msg_id, sock_p);
      break;
  }
}

void Kademlia::_erase_node(const node_t &peer) {
  auto bucket_key = to_bucket_key(peer.uuid);

  auto lock = buckets.lock();

  auto bucket_it = buckets->find(bucket_key);

  if(bucket_it == std::end(buckets.raw)) {
    // already removed

    return;
  }

  auto &bucket = bucket_it->second;
  auto pos = std::find_if(std::begin(bucket), std::end(bucket),
                          [&peer](const auto &n) { return n.first.uuid == peer.uuid; });

  if(pos == std::end(bucket)) {
    return;
  }

  if(bucket.size() <= 1) {
    buckets->erase(bucket_it);
  }

  bucket.erase(pos);
}

void Kademlia::_update_buckets(node_t &peer) {
  auto bucket_key = to_bucket_key(peer.uuid);

  auto lock = buckets.lock();

  auto bucket = map_find(buckets.raw, bucket_key);
  if(!bucket) {
    bucket = &(buckets->emplace(bucket_key, std::vector<std::pair<node_t, bool>> { }).first->second);
  }

  auto pos = std::find_if(std::begin(*bucket), std::end(*bucket),
                          [&peer](const auto &n) { return n.first.uuid == peer.uuid; });

  // possibly add node to the bucket
  if(pos == std::end(*bucket)) {
    if(bucket->size() < max_bucket_size) {
      bucket->emplace_back(std::move(peer), false);
    } else {
      // PING least-recently seen peer
      // if a PING already in progress, ping next least-recently seen peer

      auto it = std::find_if(std::begin(pending), std::end(pending),
                             [&peer](const auto &n) { return n.uuid == peer.uuid; });

      /* if node is already pending for bucket, do nothing */
      if(it != std::end(pending)) {
        *it;
      }

      // find least-recently seen node that is not pinged
      std::decay_t<decltype(*bucket)>::iterator least_recently_seen;
      for(least_recently_seen = std::begin(*bucket); least_recently_seen != std::end(*bucket); ++least_recently_seen) {
        if(!least_recently_seen->second) {
          break;
        }
      }

      auto peer_p = &peer;
      // if we haven't run out of least-recently seen
      if(least_recently_seen != std::end(*bucket)) {
        /* else, node is now pending */
        peer_p = &pending.emplace_back(std::move(peer));
      }

      TUPLE_2D_REF(old_node, is_pending, *least_recently_seen);

      auto key = msg_id_t::generate();

      auto old_sock = __get_socket(sockets, old_node.addr);
      if(__push(old_sock, key, uuid, __kademlia_e::PING)) {
        print(error, "Couldn't ping [", util::hex(old_node.uuid), "] --> ", err::current());

        // This error shouldn't be happening
        std::abort();
      }

      is_pending = true;

      mailbox.pending(server.tasks(),
        &Kademlia::_on_pending_response, &Kademlia::_on_pending_timeout,
        response_timeout, key,
        this, bucket_key, old_node, *peer_p);
    }
  }

  // move pos to the back of the bucket
  else {
    std::rotate(pos, pos + 1, std::end(*bucket));
  }
}

std::pair<file::ip_addr_t, uuid_t> Kademlia::addr() {
  return { get_multiplex(server).local_addr(), uuid };
}

void Kademlia::_poll() {
  if(!server.isRunning()) {
    return;
  }

  poll.poll();

  server.tasks().pushDelayed(&Kademlia::_poll, 0ms, this);
}
}
