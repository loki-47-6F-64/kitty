//
// Created by loki on 5-4-19.
//

#ifndef T_MAN_CONFIG_H
#define T_MAN_CONFIG_H

#ifdef KITTY_P2P_SIM
#include <kitty/p2p/pj_fake/nath.h>
#include <kitty/p2p/pj_fake/ice_trans.h>
#include <kitty/p2p/pj_fake/pool.h>
#else
#include <kitty/p2p/pj/nath.h>
#include <kitty/p2p/pj/ice_trans.h>
#include <kitty/p2p/pj/pool.h>
#endif

#endif //T_MAN_CONFIG_H
