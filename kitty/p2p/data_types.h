//
// Created by loki on 17-2-19.
//

#ifndef T_MAN_DATA_TYPES_H
#define T_MAN_DATA_TYPES_H

#include <tuple>
#include <vector>
#include <kitty/p2p/config.h>

namespace p2p::data {

using creds_t  = std::tuple<std::string, std::string>;
using ip_addr_t = std::tuple<std::string, std::uint16_t>;

// type, foundation, priority, comp_id
using ice_sess_cand_t = std::tuple<pj::ice_cand_type_t, ip_addr_t, std::string, std::uint32_t, std::uint8_t>;
using remote_t        = std::tuple<creds_t, std::vector<ice_sess_cand_t>>;

pj::remote_buf_t unpack(remote_t &&);
remote_t pack(const pj::creds_t &, const std::vector<pj::ice_sess_cand_t> &);
}

#endif //T_MAN_DATA_TYPES_H
