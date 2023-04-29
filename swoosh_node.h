#ifndef SWOOSH_NODE_H_FILE
#define SWOOSH_NODE_H_FILE

#include <cstdint>
#include <string>
#include <map>
#include <mutex>

#include "swoosh_local_data.h"
#include "swoosh_remote_data.h"
#include "swoosh_data_store.h"

class SwooshNodeClient {
  friend class SwooshNode;

protected:
  virtual void OnNetReceivedData(SwooshRemoteData *data) = 0;
  virtual void OnNetDataDownloading(SwooshRemotePermanentData *data, double progress) = 0;
  virtual void OnNetDataDownloaded(SwooshRemotePermanentData *data, bool success) = 0;
  virtual void OnNetNotify(const std::string &message) = 0;
};

class SwooshNode {
private:
  SwooshNodeClient &client;
  SwooshDataStore local_data_store;
  uint32_t next_message_id;

  void StartUDPServer();
  void StartTCPServer();
  void StartDataCollector();

  static bool running;
  static int OnBeaconReceived(net_msg_beacon *beacon, void *user_data);
  static void OnMessageRequested(net_socket *sock, void *user_data);
  static uint32_t MakeClientId();
  static void Sleep(uint32_t msec);

  void HandleMessageRequest(net_socket *sock);
  void RequestMessage(net_msg_beacon *beacon);

public:
  SwooshNode(SwooshNodeClient &client, int server_udp_port, int server_tcp_port, bool use_ipv6) : client(client) {
    running = true;
    next_message_id = 1;
    net_setup(server_udp_port, server_tcp_port, use_ipv6);
    StartUDPServer();
    StartTCPServer();
    StartDataCollector();
  }
  uint32_t GenerateMessageId() { return next_message_id++; }
  void Stop() { running = false; local_data_store.Stop(); }
  void SendDataBeacon(uint32_t message_id);
  void ReceiveDataContent(SwooshRemotePermanentData *data, std::string local_path);
  bool BeaconsAreEqual(net_msg_beacon *beacon1, net_msg_beacon *beacon2);

  void AddLocalData(SwooshLocalData *data) { local_data_store.Store(data); }
  void ReleaseLocalData(SwooshLocalData *data) { local_data_store.Release(data->GetMessageId()); }

  static uint64_t GetTime(uint32_t msec_in_future);
};

#endif /* SWOOSH_NODE_H_FILE */
