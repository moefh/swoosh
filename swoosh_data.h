#ifndef SWOOSH_DATA_H_FILE
#define SWOOSH_DATA_H_FILE

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <iterator>
#include <memory>
#include <functional>

#include <wx/filefn.h>

#include "network.h"
#include "util.h"

#define SWOOSH_DATA_ALWAYS_VALID ((uint64_t) -1)

enum {
  SWOOSH_DATA_TEXT = NET_MAKE_MAGIC('T', 'e', 'x', 't'),
  SWOOSH_DATA_FILE = NET_MAKE_MAGIC('F', 'i', 'l', 'e'),
  SWOOSH_DATA_DIR  = NET_MAKE_MAGIC('D', 'i', 'r', '/'),
};

class SwooshData {
  friend class SwooshNode;

protected:
  net_msg_beacon *beacon;
  uint64_t valid_until;
  int num_refs;

  static SwooshData *ReceiveData(net_msg_beacon *beacon, uint32_t type_id, net_socket *sock);

  virtual int SendContentHead(net_socket *sock) = 0;
  virtual int SendContentBody(net_socket *sock) = 0;
  virtual bool Download(std::string local_path, std::function<void(double)> progress) = 0;

  std::string *ReceiveString(net_socket *sock, size_t max_size);
  int SendString(net_socket *sock, const std::string &str);

public:
  SwooshData(net_msg_beacon *beacon) : beacon(beacon), valid_until(SWOOSH_DATA_ALWAYS_VALID), num_refs(0) {}
  SwooshData(uint64_t valid_until) : beacon(nullptr), valid_until(valid_until), num_refs(0) {}
  virtual ~SwooshData() {
    net_free_beacon(beacon);
  }

  net_msg_beacon *GetBeacon() { return beacon; }

  virtual bool IsGood() = 0;
  
  void AddRef() { num_refs++; }
  void RemoveRef() { num_refs--; }
  bool hasRefs() { return num_refs != 0; }

  uint64_t GetValidUntil() { return valid_until; }
  bool IsExpiredAt(uint64_t time) { return (valid_until != SWOOSH_DATA_ALWAYS_VALID) && (valid_until < time); }
};

class SwooshTextData : public SwooshData {
protected:
  bool is_good;
  std::string text;

  //virtual int GetTypeId() { return SWOOSH_DATA_TEXT; };
  virtual int SendContentHead(net_socket *sock);
  virtual int SendContentBody(net_socket *sock);
  virtual bool Download(std::string local_path, std::function<void(double)> progress);

public:
  SwooshTextData(uint64_t valid_until, const std::string &str)
    : SwooshData(valid_until), is_good(true), text(str) {
  };
  SwooshTextData(net_msg_beacon *beacon, net_socket *sock);

  virtual ~SwooshTextData() {}

  virtual bool IsGood() { return is_good; };
  virtual std::string &GetText() { return text; }
};

class SwooshFileData : public SwooshData {
protected:
  bool is_good;
  std::string file_name;
  uint32_t file_size;

  //virtual int GetTypeId() { return SWOOSH_DATA_FILE; };
  virtual int SendContentHead(net_socket *sock);
  virtual int SendContentBody(net_socket *sock);
  virtual bool Download(std::string local_path, std::function<void(double)> progress);

public:
  SwooshFileData(uint64_t valid_until, const std::string &file_name);
  SwooshFileData(net_msg_beacon *beacon, net_socket *sock);

  virtual ~SwooshFileData() {}

  virtual bool IsGood() { return is_good; };
  virtual std::string &GetFileName() { return file_name; }
  virtual uint32_t GetFileSize() { return file_size; }
};

#endif /* SOOSH_DATA_H_FILE */
