//
// Created by loki on 29-1-19.
//

#ifndef T_MAN_PACK_H
#define T_MAN_PACK_H

#include "../../json/single_include/nlohmann/json.hpp"
#include "kitty/p2p/pj/ice_trans.h"
#include "kitty/p2p/uuid.h"

namespace p2p {

std::optional<nlohmann::json> pack_remote(const pj::remote_t &remote);
std::vector<uuid_t> unpack_peers(const nlohmann::json &json);
std::optional<pj::remote_buf_t> unpack_remote(const nlohmann::json &json);

}
#endif //T_MAN_PACK_H
