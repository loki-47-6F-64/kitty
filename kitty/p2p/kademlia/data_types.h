//
// Created by loki on 14-4-19.
//

#ifndef T_MAN_DATA_TYPES_H
#define T_MAN_DATA_TYPES_H

#include <tuple>
#include <kitty/p2p/uuid.h>
#include <kitty/p2p/kademlia/kademlia.h>

namespace data {

using ip_addr_t = std::tuple<std::uint32_t, std::uint16_t>;
using node_t = std::tuple<ip_addr_t, p2p::uuid_t>;

inline node_t pack(const p2p::node_t &node) {
  return {
    node.addr.pack(),
    node.uuid
  };
}

inline p2p::node_t unpack(const node_t &node) {
  return {
    std::get<1>(node),
    file::ip_addr_buf_t::unpack(std::get<0>(node))
  };
}

}

#endif //T_MAN_DATA_TYPES_H
