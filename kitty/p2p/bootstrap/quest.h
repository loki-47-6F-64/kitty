//
// Created by loki on 8-2-19.
//

#ifndef T_MAN_QUEST_H
#define T_MAN_QUEST_H

#include <map>
#include <kitty/file/io_stream.h>
#include <kitty/p2p/uuid.h>

namespace p2p::bootstrap {
void handle_quest(file::io &client);
std::map<uuid_t, file::io> &peers();
file::poll_t<file::io, uuid_t> &poll();
}
#endif //T_MAN_QUEST_H
