#include <cstring>
#include <mutex>

#include <unistd.h>
#include <sys/socket.h>

#include <netdb.h>

#include <openssl/err.h>

#include <kitty/err/err.h>
#include <kitty/ssl/ssl.h>

using namespace std::literals;

// Special exception for gai_strerror
namespace ssl {
std::unique_ptr<std::mutex[]> lock;

static int loadCertificates(Context& ctx, const char *caPath, const char *certPath, const char *keyPath, bool verify) {
  
  if(certPath && keyPath) {
    if(SSL_CTX_use_certificate_file(ctx.get(), certPath, SSL_FILETYPE_PEM) != 1) {
      return -1;
    }

    if(SSL_CTX_use_PrivateKey_file(ctx.get(), keyPath, SSL_FILETYPE_PEM) != 1) {
      return -1;
    }

    if(SSL_CTX_check_private_key(ctx.get()) != 1) {
      return -1;
    }
  }

  if(verify) {
    if(SSL_CTX_load_verify_locations(ctx.get(), caPath, nullptr) != 1) {
      return -1;
    }
    
    SSL_CTX_set_verify(ctx.get(), SSL_VERIFY_PEER, nullptr);
    SSL_CTX_set_verify_depth(ctx.get(), 16);
  }
  else {
    SSL_CTX_set_verify(ctx.get(), SSL_VERIFY_NONE, nullptr);
  }

  return 0;
}

Context init_ctx_server(const char *caPath, const char *certPath, const char *keyPath, bool verify) {
  Context ctx(SSL_CTX_new(DTLS_server_method()));

  if(loadCertificates(ctx, caPath, certPath, keyPath, verify)) {
    err::code = err::LIB_SSL;

    return {};
  }

  return ctx;
}

Context init_ctx_server(const std::string& caPath, const std::string& certPath, const std::string& keyPath, bool verify) {
  return init_ctx_server(caPath.c_str(), certPath.c_str(), keyPath.c_str(), verify);
}

Context init_ctx_client(const char *caPath, const char *certPath, const char *keyPath) {
  Context ctx(SSL_CTX_new(DTLS_client_method()));

  if(loadCertificates(ctx, caPath, certPath, keyPath, true)) {
    err::code = err::LIB_SSL;

    return {};
  }

  return ctx;
}

Context init_ctx_client(const std::string &caPath, const std::string& certPath, const std::string& keyPath) {
  return init_ctx_server(caPath.c_str(), certPath.c_str(), keyPath.c_str());
}

Context init_ctx_client(const char *caPath) {
  Context ctx(SSL_CTX_new(DTLS_client_method()));
  
  if(loadCertificates(ctx, caPath, nullptr, nullptr, true)) {
    err::code = err::LIB_SSL;
    
    return {};
  }
  
  return ctx;
}

Context init_ctx_client(const std::string& caPath) {
  return init_ctx_client(caPath.c_str());
}

file::ssl connect(Context &ctx, const char *hostname, const char* port) {
  constexpr std::chrono::seconds timeout { 0 };

  int serverFd = socket(AF_INET, SOCK_STREAM, 0);

  addrinfo hints;
  addrinfo *server;

  std::memset(&hints, 0, sizeof (hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if(int err = getaddrinfo(hostname, port, &hints, &server)) {
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
  constexpr std::chrono::seconds timeout { 3 };

  file::ssl socket(timeout, ctx, fd);
  if(auto ret = SSL_accept(socket.getStream()._ssl.get()); ret != 1) {
    socket.seal();

    if(ret == 0) {
      err::set("The TLS/SSL handshake was not successful, but was shut down controlled and by the specifications of the TLS/SSL protocol."sv);
    }
    else {
      err::code = err::LIB_SSL;
    }
  }

  return socket;
}
}
