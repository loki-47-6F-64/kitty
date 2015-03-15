#include <mutex>

#include <unistd.h>
#include <sys/socket.h>

#include <netdb.h>

#include <kitty/err/err.h>
#include <kitty/ssl/ssl.h>
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

  char *pos = nullptr, *ch = cert->name;

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

  int serverFd = socket(AF_INET, SOCK_STREAM, 0);

  addrinfo hints;
  addrinfo *server;

  memset(&hints, 0, sizeof (hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if(int err = getaddrinfo(hostname, port, &hints, &server)) {
    err::code = err::LIB_USER;
    err::set(gai_strerror(err));
    return file::ssl();
  }


  if(connect(serverFd, server->ai_addr, server->ai_addrlen)) {
    freeaddrinfo(server);

    err::code = err::LIB_SYS;
    return file::ssl();
  }

  freeaddrinfo(server);

  file::ssl ssl_tmp(timeout, ctx, serverFd);
  

  if(SSL_connect(ssl_tmp.getStream()._ssl.get()) != 1) {
    err::code = err::LIB_SSL;
    return file::ssl();
  }

  return ssl_tmp;
}

file::ssl accept(ssl::Context &ctx, int fd) {
  constexpr long timeout = 3000 * 1000;

  file::ssl socket(timeout, ctx, fd);
  if(SSL_accept(socket.getStream()._ssl.get()) != 1) {
    socket.seal();
  }

  return socket;
}
}