//
// Created by loki on 27-1-19.
//

#include <kitty/util/set.h>
#include <kitty/p2p/pj/dns_resolv.h>
#include "dns_resolv.h"


namespace p2p::pj {

DNSResolv::DNSResolv(pool_t &pool, TimerHeap &timer_heap, IOQueue &io_queue) {
  pj_dns_resolver *resolv;

  pj_dns_resolver_create(pool->factory, nullptr, 0, timer_heap.raw(), io_queue.raw(), &resolv);
  _dns_resolv.reset(resolv);
}

status_t DNSResolv::set_ns(const std::vector<std::string_view> &_servers) {
  auto servers = util::map(_servers, [](const auto &server) { return string(server); });

  return pj_dns_resolver_set_ns(_dns_resolv.get(), servers.size(), servers.data(), nullptr);
}

void detect_nat_dns_cb(void *data, status_t status, pj_dns_parsed_packet *response) {
  auto &alarm = *((util::Alarm<std::variant<status_t, sockaddr>>*)data);

  if(status != success) {
    alarm.status() = status;
    alarm.ring();

    return;
  }

  pj_dns_addr_record record;
  pj_dns_parse_addr_response(response, &record);

  if(record.addr_count > 0) {
    sockaddr addr;

    pj_sockaddr_cp(&addr, record.addr);

    alarm.status() = addr;
    alarm.ring();

    return;
  }

  alarm.status() = status;
}

status_t DNSResolv::resolv(util::Alarm<std::variant<status_t, sockaddr>> &alarm, std::string_view hostname) {
  auto str = string(hostname);

  return pj_dns_resolver_start_query(
    _dns_resolv.get(),
    &str,
    PJ_DNS_TYPE_A,
    0,
    &detect_nat_dns_cb,
    &alarm,
    nullptr
  );
}
}