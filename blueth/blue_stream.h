#ifndef VIKING_BLUE_STREAM_H
#define VIKING_BLUE_STREAM_H

#include "file/io_stream.h"
#include "blueth.h"
namespace file {
namespace stream {
class blueth : public io {
  bt::HCI *_hci;
  bt::device *_dev;
  
public:
  void open(int fd, bt::HCI& hci, bt::device& dev);
  
  void seal();
};
}

typedef FD<stream::blueth> blueth;
}

#endif