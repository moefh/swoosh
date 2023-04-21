#include "swoosh_sender.h"

void SwooshSender::Send(net_socket* sock)
{
  threads.emplace_back(std::thread {[this, sock] {
    data->Send(sock);
    net_close_socket(sock);
  }});
}

void SwooshSender::WaitFinished()
{
  for (auto& thread : threads)  {
    thread.join();
  }
}
