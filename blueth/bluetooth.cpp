#include <algorithm>

#include <unistd.h>
#include <sys/poll.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "blueth.h"
#include "ble.h"

#include "util/utility.h"
#include "err/err.h"

namespace bt {

constexpr uint8_t MAX_BROADCAST_DATA = 31;
HCI::HCI() {
  _dev_id = hci_get_route(&this->bdaddr);

  _hci_sock = hci_open_dev(_dev_id);
  if (_dev_id < 0 || _hci_sock < 0) {
    err::code = err::LIB_SYS;
  }

  hci_devba(_dev_id, &this->bdaddr);
  
  // For some reason, the first call to advertise always fails.
  advertise(false, nullptr);
}

HCI::HCI(const char *bdaddr) {
  str2ba(bdaddr, &this->bdaddr);
  _dev_id = hci_get_route(&this->bdaddr);

  _hci_sock = hci_open_dev(_dev_id);
  if (_dev_id < 0 || _hci_sock < 0) {
    err::code = err::LIB_SYS;
  }
}

HCI::HCI(HCI && dev) {
  this->operator =(std::move(dev));
}

void HCI::operator =(HCI && dev) {
  this->bdaddr = dev.bdaddr;
  this->_dev_id = dev._dev_id;
  this->_hci_sock = dev._hci_sock;

  dev._dev_id = dev._hci_sock = -1;
}

HCI::~HCI() {
  if (_hci_sock != -1) {
    close(_hci_sock);
  }

  if (_dev_id != -1) {
    close(_dev_id);
  }
}

int HCI::advertise(bool enable, const char *name) {
  if (hci_le_set_advertise_enable(_hci_sock, enable, 1000)) {
    err::code = err::LIB_SYS;

    return -1;
  }

  if (name) {
    uint8_t data_buf[256];

    uint8_t name_len = strlen(name);
    uint8_t data_len = name_len + 2;

    data_buf[0] = name_len + 1;
    data_buf[1] = 0x08;
    memcpy(&data_buf[2], name, name_len);

    return _setResponseData(data_buf, data_len);
  }

  return 0;
}

int HCI::broadcast(const std::vector<Uuid> &uuids) {
  std::vector<Uuid> uuids16, uuids128;

  for (auto & uuid : uuids) {
    if (uuid.type == Uuid::BT_UUID16) {
      uuids16.push_back(uuid);
    }
    else {
      uuids128.push_back(uuid);
    }
  }

  std::vector<uint8_t> data;

  data.push_back(0x02);
  data.push_back(0x01);
  data.push_back(0x05);

  if (!uuids16.empty()) {
    const uint8_t data_len = 1 + 2 * uuids16.size();
    
    data.push_back(data_len);
    data.push_back(0x03);
    for (auto & uuid : uuids16) {
      util::append_struct(data, uuid.value.u16);
    }
  }

  if (!uuids16.empty()) {
    const uint8_t data_len = 1 + 16 * uuids128.size();
    
    data.push_back(data_len);
    data.push_back(0x06);
    for (auto & uuid : uuids128) {
      util::append_struct(data, uuid.value.u128);
    }
  }

  return broadcast(data.data(), data.size());
}

int HCI::broadcast(uint8_t *data, uint8_t length) {
  if (length > MAX_BROADCAST_DATA) {
    err::code = err::OUT_OF_BOUNDS;
    return -1;
  }


  hci_request rq;
  le_set_advertising_data_cp data_cp;
  uint8_t status;

  memset(&data_cp, 0, sizeof(data_cp));
  data_cp.length = length;
  memcpy(&data_cp.data, data, sizeof(data_cp.data));

  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
  rq.cparam = &data_cp;
  rq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  if (hci_send_req(_hci_sock, &rq, 1000) < 0) {
    err::code = err::LIB_SYS;
    return -1;
  }

  if (status) {
    err::code = err::INPUT_OUTPUT;
    return -1;
  }

  return 0;
}

int HCI::_setResponseData(uint8_t *data, uint8_t length) {
  hci_request rq;
  le_set_scan_response_data_cp data_cp;
  uint8_t status;

  memset(&data_cp, 0, sizeof(data_cp));
  data_cp.length = length;
  memcpy(&data_cp.data, data, sizeof(data_cp.data));

  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_SCAN_RESPONSE_DATA;
  rq.cparam = &data_cp;
  rq.clen = LE_SET_SCAN_RESPONSE_DATA_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  if (hci_send_req(_hci_sock, &rq, 1000) < 0) {
    err::code = err::LIB_SYS;
    return -1;
  }

  if (status) {
    err::code = err::INPUT_OUTPUT;
    return -1;
  }

  return 0;
}

}

