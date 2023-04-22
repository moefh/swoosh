#ifndef SWOOSH_DATA_H_FILE
#define SWOOSH_DATA_H_FILE

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <iterator>

#include "network.h"

enum {
  SWOOSH_DATA_TEXT = NET_MAKE_MAGIC('T', 'e', 'x', 't'),
};

class SwooshData {
protected:
  virtual int GetTypeId() = 0;
  virtual int SendContent(net_socket *sock) = 0;

public:
  virtual ~SwooshData() = default;

  virtual bool IsGood() = 0;
  
  int Send(net_socket *sock) {
    if (net_send_u32(sock, GetTypeId()) != 0) {
      return -1;
    }
    return SendContent(sock);
  };

  static SwooshData *ReceiveData(net_socket *sock, uint32_t type_id);
};

class SwooshTextData : public SwooshData {
protected:
  bool is_good;
  std::string text;

  virtual int GetTypeId() { return SWOOSH_DATA_TEXT; };
  virtual int SendContent(net_socket *sock);

public:
  SwooshTextData(const std::string &str) : is_good(true), text(str) {};
  SwooshTextData(net_socket *sock);

  virtual ~SwooshTextData() = default;

  virtual bool IsGood() { return is_good; };
  virtual std::string &GetText() { return text; }
};

#endif /* SOOSH_DATA_H_FILE */
