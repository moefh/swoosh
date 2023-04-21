#ifndef SWOOSH_DATA_FILE
#define SWOOSH_DATA_FILE

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <iterator>

#include "network.h"

class SwooshData {
public:
  virtual int Send(net_socket *sock) = 0;
  virtual ~SwooshData() = default;
};

class SwooshTextData : public SwooshData {
protected:
  std::vector<unsigned char> data;

public:
  SwooshTextData(std::vector<unsigned char> data) : data(data) {}

  SwooshTextData(const std::string &str) {
    data.reserve(str.size());
    std::transform(std::begin(str), std::end(str), std::back_inserter(data), [](char c){
      return (unsigned char) c;
    });
  }

  virtual int Send(net_socket *sock);
  virtual ~SwooshTextData() = default;
};

#endif /* SOOSH_DATA_FILE */
