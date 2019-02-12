/*
 * File:   ble.h
 * Author: loki
 *
 * Created on July 20, 2014, 1:34 PM
 */

#ifndef BDTRACKER_BLE_H
#define  BDTRACKER_BLE_H

#include <vector>
#include <functional>

#include <kitty/blueth/uuid.h>
#include <kitty/util/optional.h>
namespace server {
  struct blue_client_t;
}
namespace bt {
struct Session {};

class Uuid : public bt_uuid_t {
public:
  Uuid(const char *uuid);
  Uuid() = default;

  int compare(Uuid &uuid) {
    return bt_uuid_cmp(this, &uuid);
  }
};

typedef enum : char {
  NONE                   = 0x00,
  READ                   = 0x02,
  WRITE_WITHOUT_RESPONSE = 0x04,
  WRITE                  = 0x08,
  NOTIFY                 = 0x10
} props_t;

struct Descriptor {
  Uuid uuid;
  uint16_t handle;
  uint16_t value;

  uint8_t properties;
  uint8_t secure;

  std::vector<uint8_t> data;
  Descriptor(
    const char *uuid,
    std::vector<uint8_t> && data,
    props_t properties = NONE, props_t secure = NONE
  );
};

struct Characteristic {
  props_t properties, secure;
  Uuid uuid;

  uint16_t startHandle;
  uint16_t valueHandle;

  // Callback for reading, size_t offset, size_t mtu, void *session
  typedef std::function<util::Optional<std::vector<uint8_t>>(uint16_t, uint16_t, Session*)> read_func_type;
  typedef std::function<int(std::vector<uint8_t>&&, Session*)> write_func_type;
  
  read_func_type  readCallback;
  write_func_type writeCallback;

  std::vector<Descriptor> descriptors;
  std::vector<uint8_t> data;

  Characteristic(
    const char *uuid,
    std::vector<uint8_t> && data,
    read_func_type  && readCallback,
    write_func_type && writeCallback,
    std::vector<Descriptor> && descriptors,
    props_t properties = NONE, props_t secure = NONE
  );

  bool secureRead();
  bool secureWrite();

  bool canRead();
  bool canWrite();
  bool canWriteWithoutResponse();
  bool canNotify();
};

struct Service {
  Uuid uuid;

  uint16_t startHandle;
  uint16_t endHandle;

  std::vector<Characteristic> characteristics;

  Service(const char *uuid, std::vector<Characteristic> && characteristics);
};

struct ReadByTypeReq;
struct Handle {

  enum {
    SERVICE,
    CHARACTERISTIC,
    CHARACTERISTICVALUE,
    DESCRIPTOR
  } type;

  union {
    Service        *service;
    Characteristic *characteristic;
    Descriptor     *descriptor;
  };
};

class Profile {
  std::vector<Handle> _handles;
  std::vector<Service> _services;

public:
  Profile(std::vector<Service> && services);

  int main(server::blue_client_t &client) const;

private:
  std::vector<uint8_t> _readByGroup(server::blue_client_t &client) const;
  std::vector<uint8_t> _readByType(server::blue_client_t &client) const;
  std::vector<uint8_t> _findByType(server::blue_client_t &client) const;
  std::vector<uint8_t> _findInfo(server::blue_client_t &client) const;
  std::vector<uint8_t> _read(server::blue_client_t &client, uint8_t requestType) const;
  std::vector<uint8_t> _write(server::blue_client_t &client, uint8_t requestType) const;
  std::vector<uint8_t> _mtu(server::blue_client_t &client) const;

  std::vector<uint8_t> _readByTypeMeta(server::blue_client_t &client, bt::ReadByTypeReq &req) const;
};
}

#endif   /* BLE_H */

