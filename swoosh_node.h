#ifndef SWOOSH_NODE_H_FILE
#define SWOOSH_NODE_H_FILE

#include <string>
#include <cstdint>

#include "swoosh_data.h"
#include "network.h"

class SwooshNodeClient {
public:
  virtual void OnNetReceivedText(const std::string &text, const std::string &host, int port) = 0;
  virtual void OnNetNotify(const std::string &message) = 0;
};

class SwooshNode {
private:
  SwooshNodeClient &client;
  uint32_t client_id;
  int server_udp_port;
  int first_tcp_port;
  int last_tcp_port;
  bool use_ipv6;

  void StartServer();

  static bool running;
  static int ServerRecvCallback(net_msg_beacon *beacon, void *user_data);
  static void SendDataCallback(struct net_socket *sock, void *user_data);
  static uint32_t MakeClientId();

public:
  SwooshNode(SwooshNodeClient &client, int server_udp_port, int first_tcp_port, int last_tcp_port, bool use_ipv6)
      : client(client), server_udp_port(server_udp_port),
        first_tcp_port(first_tcp_port), last_tcp_port(last_tcp_port), use_ipv6(use_ipv6) {
    running = true;
    client_id = MakeClientId();
    StartServer();
  }
  void Stop() { running = false; }
  void SendData(SwooshData *data);
  void SendText(const std::string &text) { SendData(new SwooshTextData(text)); }
};

#endif /* SWOOSH_NODE_H_FILE */
