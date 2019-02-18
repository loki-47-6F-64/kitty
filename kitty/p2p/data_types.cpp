//
// Created by loki on 17-2-19.
//

#include <kitty/util/set.h>
#include <kitty/p2p/data_types.h>

namespace p2p::data {

pj::creds_buf_t unpack(creds_t &&creds) {
  return pj::creds_buf_t {
    std::move(std::get<0>(creds)),
    std::move(std::get<1>(creds))
  };
}

pj::ip_addr_t unpack(const ip_addr_t &addr) {
  return pj::ip_addr_t {
    std::get<0>(addr),
    std::get<1>(addr)
  };
}

creds_t  pack(const pj::creds_t &creds) {
  return creds_t {
    creds.ufrag,
    creds.passwd
  };
}

ip_addr_t pack(const pj::ip_addr_t &addr) {
  return ip_addr_t {
    addr.ip,
    addr.port
  };
}

pj::ice_sess_cand_t unpack(const ice_sess_cand_t &cand_d) {
  pj::ice_sess_cand_t cand {};

  cand.type       = std::get<0>(cand_d);
  cand.addr       = *unpack(std::get<1>(cand_d)).to_sockaddr();
  cand.foundation = pj::string(std::get<2>(cand_d));
  cand.prio       = std::get<3>(cand_d);
  cand.comp_id    = std::get<4>(cand_d);

  return cand;
}

ice_sess_cand_t pack(const pj::ice_sess_cand_t &cand) {
  std::vector<char> buf;
  return ice_sess_cand_t {
    cand.type,
    pack(pj::ip_addr_t::from_sockaddr_t(buf, &cand.addr)),
    pj::string(cand.foundation),
    cand.prio,
    cand.comp_id
  };
}

pj::remote_buf_t unpack(remote_t &&remote) {
  return {
    unpack(std::move(std::get<0>(remote))),
    util::map(std::get<1>(remote), [](auto &c) { return unpack(c); })
  };
}

remote_t pack(const pj::creds_t &creds, const std::vector<pj::ice_sess_cand_t> &cands) {
  return remote_t {
    pack(creds),
    util::map(cands, [](auto &c) { return pack(c); })
  };
}

}