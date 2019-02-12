#ifndef SSL_CLIENT_H
#define SSL_CLIENT_H

#include <memory>

#include <arpa/inet.h>
#include <kitty/ssl/ssl_stream.h>
#include <kitty/ssl/ssl.h>
#include <kitty/server/server.h>
namespace server {
struct ssl_client_t {
  struct member_t : public tcp_client_t::member_t {
    member_t(::ssl::Context &&ctx, const sockaddr_in6 &in) : tcp_client_t::member_t(in), ctx { std::move(ctx) } {}

    ::ssl::Context ctx;
  };

  std::unique_ptr<file::ssl> socket;
  std::string ip_addr;
};

typedef Server<ssl_client_t> ssl;
}

#endif