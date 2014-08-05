#include <mutex>

#include <unistd.h>
#include <sys/socket.h>

#include <netdb.h>

#include "err.h"
#include "_ssl.h"

// Special exception for gai_strerror
namespace err {
extern void set(const char *err);
}

namespace ssl {
std::unique_ptr<std::mutex[] > lock;

void crypto_lock(int mode, int n, const char *file, int line) {
  if(mode & CRYPTO_LOCK) {
    lock[n].lock();
  }
  else {
    lock[n].unlock();
  }
}

int loadCertificates(Context& ctx, const char *caPath, const char *certPath, const char *keyPath) {
  if(SSL_CTX_use_certificate_file(ctx.get(), certPath, SSL_FILETYPE_PEM) != 1) {
    return -1;
  }

  if(SSL_CTX_use_PrivateKey_file(ctx.get(), keyPath, SSL_FILETYPE_PEM) != 1) {
    return -1;
  }

  if(SSL_CTX_check_private_key(ctx.get()) != 1) {
    return -1;
  }

  if(SSL_CTX_load_verify_locations(ctx.get(), caPath, nullptr) != 1) {
    return -1;
  }

  SSL_CTX_set_verify(ctx.get(), SSL_VERIFY_PEER, nullptr);
  SSL_CTX_set_verify_depth(ctx.get(), 1);

  return 0;
}

std::string getCN(const SSL *ssl) {
  std::string cn;

  Certificate cert(SSL_get_peer_certificate(ssl));
  if(!cert.get()) {
    return cn;
  }

  char *pos, *ch = cert->name;

  while(*ch) {
    if(*ch == '/')
      pos = ch;
    ++ch;
  }

  // TODO: check if it is legal to use '/' in CN
  cn = ch - pos > 4 ? pos + 4 : "";

  return cn;
}

Context init_ctx_server(std::string &caPath, std::string& certPath, std::string& keyPath) {
  Context ctx(SSL_CTX_new(SSLv3_server_method()));

  if(loadCertificates(ctx, caPath.c_str(), certPath.c_str(), keyPath.c_str())) {
    err::code = err::LIB_SSL;

    ctx.release();
  }

  return ctx;
}

Context init_ctx_client(std::string &caPath, std::string& certPath, std::string& keyPath) {
  Context ctx(SSL_CTX_new(SSLv3_client_method()));

  if(loadCertificates(ctx, caPath.c_str(), certPath.c_str(), keyPath.c_str())) {
    err::code = err::LIB_SSL;

    ctx.release();
  }

  return ctx;
}

void init() {
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();

  lock = std::unique_ptr < std::mutex[]>(new std::mutex[CRYPTO_num_locks()]);
  CRYPTO_set_locking_callback(crypto_lock);
}

file::ssl connect(Context &ctx, const char *hostname, const char* port) {
  constexpr long timeout = -1;

  file::ssl sslFd;
  int serverFd = socket(AF_INET, SOCK_STREAM, 0);

  addrinfo hints;
  addrinfo *server;

  memset(&hints, 0, sizeof (hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  int err;
  if((err = getaddrinfo(hostname, port, &hints, &server))) {
    err::code = err::LIB_GAI;
    err::set(gai_strerror(err));
    return sslFd;
  }


  if(connect(serverFd, server->ai_addr, server->ai_addrlen)) {
    freeaddrinfo(server);

    err::code = err::LIB_SSL;
    return sslFd;
  }

  freeaddrinfo(server);
  SSL *ssl = SSL_new(ctx.get());

  if(ssl == nullptr) {
    err::code = err::LIB_SSL;
  }

  file::ssl ssl_tmp(timeout, ssl);
  SSL_set_fd(ssl, serverFd);

  if(SSL_connect(ssl) != 1) {
    err::code = err::LIB_SSL;
    return sslFd;
  }

  sslFd = std::move(ssl_tmp);

  return sslFd;
}

file::ssl accept(ssl::Context &ctx, int fd) {
  constexpr long timeout = 3000 * 1000;


  SSL *ssl = SSL_new(ctx.get());
  SSL_set_fd(ssl, fd);

  file::ssl socket(timeout, ssl);
  if(SSL_accept(ssl) != 1) {
    socket.seal();
  }

  return socket;
}
}