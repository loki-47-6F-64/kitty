#include "blue_stream.h"

namespace file {
namespace stream {
void blueth::open(int fd, bt::HCI& hci, bt::device& dev) {  
  _hci = &hci;
  _dev = &dev;
  
  io::open(fd);
}

void blueth::seal() {
  auto handle = _hci->getConnHandle(*_dev);
  
  if(handle) {
    _hci->disconnect(handle);
  }

  io::seal();
}

}
}
