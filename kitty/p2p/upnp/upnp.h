//
// Created by loki on 10-4-19.
//

#ifndef T_MAN_UPNP_H
#define T_MAN_UPNP_H

#include <miniupnpc.h>
#include <kitty/util/utility.h>
#include <kitty/file/tcp.h>

namespace p2p::upnp {

void __free_urls(UPNPUrls *p) noexcept;

using urls_t = util::safe_ptr<UPNPUrls, __free_urls>;
using dev_t  = util::safe_ptr<UPNPDev, freeUPNPDevlist>;

util::Either<dev_t, int> discover(std::chrono::milliseconds, std::uint16_t local_port, bool ipv6);

enum class proto_e {
  TCP,
  UDP
};

class URL {
public:
  /* UPNP_AddAnyPortMapping()
 * if desc is NULL, it will be defaulted to "kitty"
 *
 * Return values :
 * 0 : SUCCESS
 * NON ZERO : ERROR. Either an UPnP error code or an unknown error.
 *
 * List of possible UPnP errors for AddPortMapping :
 * errorCode errorDescription (short) - Description (long)
 * 402 Invalid Args - See UPnP Device Architecture section on Control.
 * 501 Action Failed - See UPnP Device Architecture section on Control.
 * 606 Action not authorized - The action requested REQUIRES authorization and
 *                             the sender was not authorized.
 * 715 WildCardNotPermittedInSrcIP - The source IP address cannot be
 *                                   wild-carded
 * 716 WildCardNotPermittedInExtPort - The external port cannot be wild-carded
 * 728 NoPortMapsAvailable - There are not enough free ports available to
 *                           complete port mapping.
 * 729 ConflictWithOtherMechanisms - Attempted port mapping is not allowed
 *                                   due to conflict with other mechanisms.
 * 732 WildCardNotPermittedInIntPort - The internal port cannot be wild-carded
 */
  int map_port(std::uint16_t localport, std::uint16_t extport, std::chrono::seconds lease, const char *descr);
  void renew(std::chrono::seconds lease, const char *descr);

  URL() noexcept = default;
  URL(URL&&) noexcept = default;

  URL&operator=(URL&&) noexcept = default;

  ~URL() noexcept;

  static std::optional<URL> url(dev_t &devices);

  file::ip_addr_t ext_addr();
  file::ip_addr_t local_addr();
private:
  IGDdatas _data;
  urls_t _urls;

  proto_e _proto;

  file::ip_addr_buf_t _wanaddr;
  file::ip_addr_buf_t _lanaddr;
};

}

#endif //T_MAN_UPNP_H
