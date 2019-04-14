//
// Created by loki on 10-4-19.
//

#include <upnpcommands.h>
#include <cstring>
#include <upnperrors.h>
#include "upnp.h"

namespace p2p::upnp {
using namespace std::literals;

constexpr auto ttl = 2;

const char *__from_proto(proto_e proto) {
  const char *p;

  switch(proto) {
    case proto_e::TCP:
      p = "TCP";
      break;
    case proto_e::UDP:
      p = "UDP";
      break;
  }

  return p;
}

util::Either<dev_t, int> discover(std::chrono::milliseconds to, std::uint16_t local_port, bool ipv6) {
  int error;

  dev_t device { upnpDiscover(to.count(), nullptr, "\0", local_port, ipv6, ttl, &error) };

  if (!device) {
    return error;
  }

  return device;
}

urls_t __make_url(dev_t &devices, IGDdatas *data, std::string &lanaddr) {
  lanaddr.resize(INET6_ADDRSTRLEN);

  urls_t urls;

  auto *urls_p = new UPNPUrls;
  auto i = UPNP_GetValidIGD(devices.get(), urls.get(), data, lanaddr.data(), lanaddr.size());

  if(!i) {
    return nullptr;
  }

  urls.reset(urls_p);

  lanaddr.resize(std::strlen(lanaddr.c_str()));
  return urls;
}

std::optional<URL> URL::url(dev_t &devices) {
  URL url;

  auto urls = __make_url(devices, &url._data, url._lanaddr.ip);

  if(!urls) {
    return std::nullopt;
  }

  url._urls = std::move(urls);

  std::string wan;
  wan.resize(INET6_ADDRSTRLEN);

  if(UPNP_GetExternalIPAddress(url._urls->controlURL, url._data.first.servicetype, wan.data())) {
    return std::nullopt;
  }

  wan.resize(std::strlen(wan.c_str()));

  url._wanaddr.ip = std::move(wan);
  return std::move(url);
}

int URL::map_port(std::uint16_t localport, std::uint16_t extport, std::chrono::seconds lease,
                   const char *descr) {
  auto lp_str    = std::to_string(localport);
  auto ep_str    = std::to_string(extport);
  auto lease_str = std::to_string(lease.count());

  std::string reserved_port;
  reserved_port.resize(6);

  descr = descr ? descr : "libkitty";

  auto ret = UPNP_AddAnyPortMapping(
    _urls->controlURL,
    _data.first.servicetype,
    ep_str.c_str(),
    lp_str.c_str(),
    _lanaddr.ip.c_str(),
    descr,
    __from_proto(_proto),
    nullptr,
    lease_str.c_str(),
    reserved_port.data());

  if(ret) {
    err::set("AddAnyPortMapping failed: "s + strupnperror(ret));

    return -1;
  }

  std::size_t _;
  _wanaddr.port = std::stoul(reserved_port, &_);
  _lanaddr.port = localport;

  return 0;
}

URL::~URL() noexcept {
  if(_urls && _wanaddr.port) {
    auto ep_str = std::to_string(_wanaddr.port);

    UPNP_DeletePortMapping(_urls->controlURL, _data.first.servicetype, ep_str.c_str(), __from_proto(_proto), nullptr);
  }
}

file::ip_addr_t URL::ext_addr() {
  return _wanaddr;
}

file::ip_addr_t URL::local_addr() {
  return _lanaddr;
}

void __free_urls(UPNPUrls *p) noexcept {
  FreeUPNPUrls(p);

  delete(p);
}

//std::optional<Multiplex> Multiplex::multiplex(dev_t &devices) {
//  auto sock = file::udp_init();
//
//  if(!sock.is_open()) {
//    return std::nullopt;
//  }
//
//  auto lp = file::sockport(sock);
//  auto ep = lp;
//
//  auto url = URL::url(devices);
//  if(!url) {
//    return std::nullopt;
//  }
//
//  if(url->map_port(lp, ep, 3600s, nullptr)) {
//    return std::nullopt;
//  }
//
//  return std::make_optional<Multiplex>(std::make_shared<file::multiplex>(std::move(sock)), std::move(*url));
//}

}