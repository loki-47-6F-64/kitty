#ifndef SSL_STREAM_H
#define SSL_STREAM_H

#include <openssl/ssl.h>
#include <kitty/file/file.h>
namespace file {
namespace stream {


class ssl {
  bool _eof;

  SSL *_ssl;
public:
  ssl();

  void operator=(ssl&& stream);
  void open(SSL *ssl);

  int operator>>(std::vector<unsigned char>& buf);
  int operator<<(std::vector<unsigned char>& buf);

  bool is_open();
  bool eof();

  void seal();

  int fd();
};
}


typedef FD<stream::ssl> ssl;
}

#endif