#include "swoosh_data.h"

#include <vector>

#include "util.h"

#define MAX_TEXT_SIZE  (512*1024*1024)

SwooshData *SwooshData::ReceiveData(net_socket *sock, uint32_t data_type_id)
{
  switch (data_type_id) {
  case SWOOSH_DATA_TEXT: return new SwooshTextData(sock);

  default:
    DebugLog("ERROR: unlnown message type id: %u (0x%x)", data_type_id, data_type_id);
    return nullptr;
  }
}

SwooshTextData::SwooshTextData(net_socket *sock) : SwooshData(0), is_good(false), text("")
{
  // read message length
  uint32_t data_len = 0;
  if (net_recv_u32(sock, &data_len) < 0) {
    DebugLog("ERROR: can't read text message size\n");
    return;
  }
  if (data_len > MAX_TEXT_SIZE) {
    DebugLog("ERROR: invalid text size: %u (max is %u)\n", data_len, MAX_TEXT_SIZE);
    return;
  }

  // read message data
  std::vector<char> data(data_len);
  int ret = net_recv_data(sock, data.data(), data_len);
  if (ret < 0) {
    DebugLog("ERROR: error receiving text data\n");
    return;
  }

  // convert to string
  text = std::string(data.begin(), data.end());
  is_good = true;
}

int SwooshTextData::SendContent(net_socket *sock)
{
  uint32_t data_size = (uint32_t) text.size();
  if (net_send_u32(sock, data_size) < 0) {
    return -1;
  }

  if (net_send_data(sock, text.data(), data_size) < 0) {
    return -1;
  }

  return 0;
}
