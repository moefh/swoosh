#include "targetver.h"
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
    swoosh_node->RequestMessage(beacon);
  }};
  receiver_thread.detach();
  return 0;
}

void SwooshNode::OnMessageRequested(net_socket *sock, void *user_data)
{
  SwooshNode *swoosh_node = (SwooshNode *) user_data;
  std::thread message_thread{[swoosh_node, sock] {
    swoosh_node->HandleMessageRequest(sock);
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
    while (running) {
      Sleep(5000);
      local_data_store.RemoveExpired(GetTime(0));
    }
  }};
  data_collector_thread.detach();
}

void SwooshNode::SendDataBeacon(uint32_t message_id)
{
  std::thread send_beacon_thread{[message_id] {
    net_send_msg_beacon(message_id);
  }};
  send_beacon_thread.detach();
}

void SwooshNode::HandleMessageRequest(net_socket *sock)
{
  // read message id
  uint32_t message_id = 0;
  if (net_recv_u32(sock, &message_id) < 0) {
    DebugLog("ERROR: can't read message id\n");
    net_close_socket(sock);
    return;
  }

  // read request type
  uint32_t request_type = 0;
  if (net_recv_u32(sock, &request_type) < 0) {
    DebugLog("ERROR: can't read request type\n");
    net_close_socket(sock);
    return;
  }

  // get data corresponding to the message id
  SwooshLocalData *data = local_data_store.Acquire(message_id, GetTime(0));
  if (!data) {
    DebugLog("ERROR: message %u not found\n", message_id);
    net_close_socket(sock);
    return;
  }

  // send data
  switch (request_type) {
  case SWOOSH_DATA_REQUEST_HEAD: data->SendContentHead(sock); break;
  case SWOOSH_DATA_REQUEST_BODY: data->SendContentBody(sock); break;
  default:
    DebugLog("ERROR: unknown request type: %u\n", request_type);
    break;
  }
  
  local_data_store.Release(message_id);
  net_close_socket(sock);
}

void SwooshNode::RequestMessage(net_msg_beacon *beacon)
{
  SwooshRemoteData *data = SwooshRemoteData::ReceiveData(beacon);
  if (!data) {
    DebugLog("ERROR: can't read message response\n");
    net_free_beacon(beacon);
    return;
  }

  // show message on UI
  if (data->IsGood()) {
    client.OnNetReceivedData(data);
  } else {
    delete data;
  }
}

void SwooshNode::ReceiveDataContent(SwooshRemoteData *data, std::string local_path)
{
  std::thread data_downloader_thread{[this, data, local_path] {
    client.OnNetDataDownloading(data, 0.0);
    bool success = data->Download(local_path, [this, data] (double progress) {
      client.OnNetDataDownloading(data, progress);
    });
    client.OnNetDataDownloaded(data, success);
  }};
  data_downloader_thread.detach();
}

bool SwooshNode::BeaconsAreEqual(net_msg_beacon *beacon1, net_msg_beacon *beacon2)
{
  return net_beacons_are_equal(beacon1, beacon2) == 1;
}
