#ifndef SWOOSH_NODE_H_FILE
#define SWOOSH_NODE_H_FILE

#include <cstdint>
#include <string>
#include <map>
#include <mutex>

#include "swoosh_data.h"
#include "swoosh_data_store.h"
#include "network.h"

class SwooshNodeClient {
public:
  virtual void OnNetReceivedData(SwooshData *data, const std::string &host, int port) = 0;
  virtual void OnNetNotify(const std::string &message) = 0;
};

class SwooshNode {
private:
  SwooshNodeClient &client;
  SwooshDataStore data_store;
  uint32_t client_id;
  uint32_t next_message_id;
  int server_udp_port;
  int server_tcp_port;
  bool use_ipv6;

  void StartUDPServer();
  void StartTCPServer();

  static bool running;
  static int OnBeaconReceived(net_msg_beacon *beacon, void *user_data);
  static void OnMessageRequested(net_socket *sock, void *user_data);
  static uint32_t MakeClientId();

public:
  SwooshNode(SwooshNodeClient &client, int server_udp_port, int server_tcp_port, bool use_ipv6)
      : client(client), server_udp_port(server_udp_port), server_tcp_port(server_tcp_port),
        use_ipv6(use_ipv6) {
    running = true;
    client_id = MakeClientId();
    next_message_id = 1;
    net_setup(client_id, server_udp_port, server_tcp_port, use_ipv6);
    StartUDPServer();
    StartTCPServer();
  }
  void Stop() { running = false; }
  void SendData(SwooshData *data);
  void SendText(const std::string &text) { SendData(new SwooshTextData(text)); }
  void SendNetMessage(net_socket *sock);
  void ReceiveNetMessage(net_socket *sock);
};

#endif /* SWOOSH_NODE_H_FILE */
