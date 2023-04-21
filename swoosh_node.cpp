
#include "swoosh_node.h"

#include <vector>
#include <random>
#include <thread>
#include <wx/wx.h>

#include "swoosh_data.h"
#include "swoosh_sender.h"
#include "util.h"

#define MAX_TRANSFER_DATA_SIZE  (512*1024*1024)

bool SwooshNode::running = false;

uint32_t SwooshNode::MakeClientId() {
  std::random_device rd;
  std::uniform_int_distribution<uint32_t> dist;
  return dist(rd);
}

void SwooshNode::SendDataCallback(struct net_socket *sock, void *user_data)
{
  if (!running) {
    return;
  }

  SwooshSender *sender = (SwooshSender *) user_data;
  sender->Send(sock);
}

int SwooshNode::ServerRecvCallback(net_msg_beacon *beacon, void *user_data)
{
  if (!running) {
    return -1;
  }
  SwooshNode *swoosh_node = (SwooshNode *) user_data;
  std::thread receiver_thread{[swoosh_node, beacon] {
    struct net_socket *sock = net_connect_to_beacon(beacon);
    if (! sock) {
      return;
    }
    uint32_t data_len = 0;
    if (net_recv_data_len(sock, &data_len) < 0) {
      DebugLog("ERROR: can't read data transfer size\n");
      net_close_socket(sock);
      return;
    }
    if (data_len > MAX_TRANSFER_DATA_SIZE) {
      DebugLog("ERROR: invalid data transfer size: %u (max is %u)\n", data_len, MAX_TRANSFER_DATA_SIZE);
      net_close_socket(sock);
      return;
    }
    std::vector<char> data(data_len, 'a');
    int ret = net_recv_data(sock, data.data(), data_len);
    net_close_socket(sock);
    if (ret < 0) {
      DebugLog("ERROR: error receiving data\n");
      return;
    }
    swoosh_node->client.OnNetReceivedText(std::string(data.data(), data.size()), "Message", 0);
  }};
  receiver_thread.detach();
  return 0;
}

void SwooshNode::StartServer()
{
  net_setup(client_id, server_udp_port, first_tcp_port, last_tcp_port, use_ipv6);
  std::thread server_thread{[this] {
    if (net_udp_server(SwooshNode::ServerRecvCallback, this) != 0) {
      client.OnNetNotify("ERROR: can't initialize network server");
    }
  }};
  server_thread.detach();
}

void SwooshNode::SendData(SwooshData *data)
{
  SwooshSender *sender = new SwooshSender(data);

  std::thread pre_send_thread{[this, sender] {
    net_msg_send(SwooshNode::SendDataCallback, sender);
    sender->WaitFinished();
    delete sender;
  }};
  pre_send_thread.detach();
}
