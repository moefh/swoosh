
#include "swoosh_node.h"

#include <vector>
#include <random>
#include <thread>
#include <wx/wx.h>
#include <wx/time.h>

#include "swoosh_data.h"
#include "util.h"

bool SwooshNode::running = false;

uint32_t SwooshNode::MakeClientId() {
  std::random_device rd;
  std::uniform_int_distribution<uint32_t> dist;
  return dist(rd);
}

void SwooshNode::Sleep(uint32_t msec)
{
  wxMilliSleep(msec);
}

uint64_t SwooshNode::GetTime(uint32_t msec_in_future)
{
  wxLongLong now = wxGetUTCTimeMillis();

  return (((uint64_t) now.GetHi()) << 32 | ((uint64_t) now.GetLo())) + msec_in_future;
}

int SwooshNode::OnBeaconReceived(net_msg_beacon *beacon, void *user_data)
{
  if (!running) {
    return -1;
  }

  SwooshNode *swoosh_node = (SwooshNode *) user_data;
  std::thread receiver_thread{[swoosh_node, beacon] {
    uint32_t message_id = net_get_beacon_message_id(beacon);
    struct net_socket *sock = net_connect_to_beacon(beacon);
    if (! sock) {
      return;
    }

    // send request for message with our ID
    if (net_send_u32(sock, message_id) != 0) {
      net_close_socket(sock);
      return;
    }

    // read message
    swoosh_node->ReceiveNetMessage(sock);
    net_close_socket(sock);
  }};
  receiver_thread.detach();
  return 0;
}

void SwooshNode::OnMessageRequested(net_socket *sock, void *user_data)
{
  SwooshNode *swoosh_node = (SwooshNode *) user_data;
  std::thread message_thread{[swoosh_node, sock] {
    swoosh_node->SendNetMessage(sock);
  }};
  message_thread.detach();
}

void SwooshNode::StartUDPServer()
{
  std::thread server_thread{[this] {
    if (net_udp_server(SwooshNode::OnBeaconReceived, this) != 0) {
      client.OnNetNotify("ERROR: can't initialize UDP server");
    }
  }};
  server_thread.detach();
}

void SwooshNode::StartTCPServer()
{
  std::thread tcp_server_thread{[this] {
    if (net_tcp_server(SwooshNode::OnMessageRequested, this) != 0) {
      client.OnNetNotify("ERROR: can't initialize TCP server");
    }
  }};
  tcp_server_thread.detach();
}

void SwooshNode::StartDataCollector()
{
  std::thread data_collector_thread{[this] {
    while (true) {
      Sleep(5000);
      uint64_t cur_time = GetTime(0);
      data_store.RemoveExpired(cur_time);
    }
  }};
  data_collector_thread.detach();
}

void SwooshNode::SendData(SwooshData *data)
{
  uint32_t message_id = next_message_id++;
  data_store.Store(message_id, data);

  std::thread pre_send_thread{[message_id] {
    net_send_msg_beacon(message_id);
  }};
  pre_send_thread.detach();
}

void SwooshNode::SendNetMessage(net_socket *sock)
{
  // read message id
  uint32_t message_id = 0;
  if (net_recv_u32(sock, &message_id) < 0) {
    DebugLog("ERROR: can't read message id\n");
    net_close_socket(sock);
    return;
  }

  // get data corresponding to the message id
  SwooshData *data = data_store.Acquire(message_id, GetTime(0));
  if (!data) {
    DebugLog("ERROR: message %u not found\n", message_id);
    net_close_socket(sock);
    return;
  }

  // send data
  data->Send(sock);
  net_close_socket(sock);
  data_store.Release(message_id);
}

void SwooshNode::ReceiveNetMessage(net_socket *sock)
{
  // read data type
  uint32_t data_type_id;
  if (net_recv_u32(sock, &data_type_id) != 0) {
    DebugLog("ERROR: can't read message type\n");
    return;
  }

  // read message
  SwooshData *data = SwooshData::ReceiveData(sock, data_type_id);

  // show message on UI
  if (data->IsGood()) {
    client.OnNetReceivedData(data, "Message", 0);
  }
  delete data;
}
