#ifndef SWOOSH_SENDER_H_FILE
#define SWOOSH_SENDER_H_FILE

#include <vector>
#include <thread>

#include "swoosh_data.h"
#include "network.h"

class SwooshSender
{
protected:
  SwooshData *data;
  std::vector<std::thread> threads;

public:
  SwooshSender(SwooshData *data) : data(data) {}
  ~SwooshSender() { delete data; }
  void Send(net_socket *sock);
  void WaitFinished();
};

#endif /* SWOOSH_SENDER_H_FILE */
