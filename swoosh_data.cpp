#include "swoosh_data.h"

int SwooshTextData::Send(net_socket *sock)
{
  uint32_t data_size = (uint32_t) data.size();
  if (net_send_data(sock, &data_size, sizeof(data_size)) < 0) {
    return -1;
  }

  if (net_send_data(sock, data.data(), data_size) < 0) {
    return -1;
  }

  return 0;
}
