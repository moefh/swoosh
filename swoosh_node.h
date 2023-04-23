#ifndef SWOOSH_NODE_H_FILE
#define SWOOSH_NODE_H_FILE

#include <cstdint>
#include <string>
#include <map>
#include <mutex>

#include "swoosh_data.h"
#include "swoosh_data_store.h"
#include "network.h"

enum {
  REQUEST_HEAD = 0,
  REQUEST_BODY = 1,
};

class SwooshNodeClient {
public:
  virtual void OnNetReceivedData(SwooshData *data) = 0;
  virtual void OnNetDataDownloading(SwooshData *data, double progress) = 0;
  virtual void OnNetDataDownloaded(SwooshData *data, bool success) = 0;
  virtual void OnNetNotify(const std::string &message) = 0;
};

class SwooshNode {
private:
  SwooshNodeClient &client;
  SwooshDataStore data_store;
  uint32_t client_id;
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
  void RequestMessage(net_socket *sock, net_msg_beacon *beacon);

public:
  SwooshNode(SwooshNodeClient &client, int server_udp_port, int server_tcp_port, bool use_ipv6) : client(client) {
    running = true;
    client_id = MakeClientId();
    next_message_id = 1;
    net_setup(client_id, server_udp_port, server_tcp_port, use_ipv6);
    StartUDPServer();
    StartTCPServer();
    StartDataCollector();
  }
  void Stop() { running = false; }
  void SendDataBeacon(SwooshData *data);
  void ReceiveDataContent(SwooshData *data, std::string local_path);

  static uint64_t GetTime(uint32_t msec_in_future);
};

#endif /* SWOOSH_NODE_H_FILE */
