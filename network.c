#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>

#include "network.h"
#include "util.h"

//#define DebugLog(...) do {} while (0)
//#define DebugDumpBytes(...) do {} while (0)

#if defined(_WIN32)
#define SETUP_WINSOCK 1
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#define close closesocket
typedef SOCKET sock_type;
#elif defined(__WIN32__)
#define SETUP_WINSOCK 1
# include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#define close closesocket
typedef SOCKET sock_type;
const char *inet_ntop(int, const void *, char *, size_t);
#else
#include <unistd.h>
# include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
typedef int sock_type;
#endif

#define BEACON_PACKET_MAX_SIZE   256
#define BEACON_PACKET_SIZE       20
#define BEACON_MAGIC             NET_MAKE_MAGIC('S', 'w', 'o', 'o')
#define BEACON_VERSION           0x00000002

#define TCP_BACKLOG         10
#define TCP_LISTEN_TIME_MS  5000

struct net_config {
  uint32_t client_id;
  int      udp_server_port;
  int      tcp_server_port;
  int      use_ipv6;
};

struct net_socket {
  sock_type sock;
};

struct net_msg_beacon {
  int      net_family;
  char     net_host[256];
  char     net_port[16];
  uint32_t client_id;
  uint32_t message_id;
};

static struct net_config config;

static void pack_u32(unsigned char *data, size_t off, uint32_t val)
{
  data[off+0] = (val >>  0) & 0xff;
  data[off+1] = (val >>  8) & 0xff;
  data[off+2] = (val >> 16) & 0xff;
  data[off+3] = (val >> 24) & 0xff;
}

static uint32_t unpack_u32(unsigned char *data, size_t off)
{
  return
    (((uint32_t) data[off+0]) <<  0) |
    (((uint32_t) data[off+1]) <<  8) |
    (((uint32_t) data[off+2]) << 16) |
    (((uint32_t) data[off+3]) << 24);
}

static char *get_address_host(struct sockaddr *addr, char *str, size_t str_size)
{
  void *addr_data;
  if (addr->sa_family == AF_INET) {
    addr_data = &(((struct sockaddr_in *)addr)->sin_addr);
  } else if (addr->sa_family == AF_INET6) {
    addr_data = &(((struct sockaddr_in6*)addr)->sin6_addr);
  } else {
    snprintf(str, str_size, "(unknown address family %d)", addr->sa_family);
    return str;
  }

  if (inet_ntop(addr->sa_family, addr_data, str, str_size) == NULL) {
    snprintf(str, str_size, "(error: %d)", errno);
  }
  return str;
}

#if 0
static int get_address_port(struct sockaddr *addr)
{
  if (addr->sa_family == AF_INET) {
    return htons(((struct sockaddr_in *) addr)->sin_port);
  }
  if (addr->sa_family == AF_INET6) {
    return htons(((struct sockaddr_in6 *) addr)->sin6_port);
  }
  return -1;
}
#endif

static sock_type open_server_socket(int type, const char *port, int use_ipv6, struct sockaddr_storage *addr, socklen_t *addr_len)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = (use_ipv6) ? AF_INET6 : AF_INET;
  hints.ai_socktype = type;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *servinfo;
  if (getaddrinfo(NULL, port, &hints, &servinfo) != 0) {
    return -1;
  }

  sock_type sock = -2;
  for (struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) {
    sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sock < 0) {
      continue;
    }

    if (bind(sock, p->ai_addr, (int) p->ai_addrlen) < 0) {
      sock = -2;
      close(sock);
      continue;
    }

    if (addr != NULL) {
      memcpy(addr, p->ai_addr, p->ai_addrlen);
    }
    if (addr_len != NULL) {
      *addr_len = (socklen_t) p->ai_addrlen;
    }
    break;
  }

  freeaddrinfo(servinfo);
  return sock;
}

static sock_type open_udp_broadcast_socket(int port, int use_ipv6, struct sockaddr_storage *addr, socklen_t *addr_len)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = (use_ipv6) ? AF_INET6 : AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  const char *broadcast_node = (use_ipv6) ? "FF02::1" : "255.255.255.255";

  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", port);

  struct addrinfo *servinfo;
  if (getaddrinfo(broadcast_node, port_str, &hints, &servinfo) != 0) {
    return -1;
  }

  sock_type sock = -2;
  for (struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) {
    sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sock < 0) {
      continue;
    }

    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcast, sizeof(broadcast)) < 0) {
      sock = -2;
      continue;
    }

    if (addr != NULL) {
      memcpy(addr, p->ai_addr, p->ai_addrlen);
    }
    if (addr_len != NULL) {
      *addr_len = (socklen_t) p->ai_addrlen;
    }
    break;
  }

  freeaddrinfo(servinfo);
  return sock;
}

int net_setup(uint32_t client_id, int udp_server_port, int tcp_server_port, int use_ipv6)
{
  config.client_id = client_id;
  config.udp_server_port = udp_server_port;
  config.tcp_server_port = tcp_server_port;
  config.use_ipv6 = use_ipv6;

#if SETUP_WINSOCK
  WORD version_wanted = MAKEWORD(2, 0);
  WSADATA wsa_data;
  return WSAStartup(version_wanted, &wsa_data);
#else
  return 0;
#endif
}

void net_close_socket(struct net_socket *sock)
{
  DebugLog("========== FREEING SOCKET %p\n", sock);
  close(sock->sock);
  free(sock);
}

int net_send_data(struct net_socket *sock, const void *data, size_t len)
{
  size_t len_left = len;
  const char *data_left = data;
  while (len_left > 0) {
    int done = send(sock->sock, data_left, (int) len_left, 0);
    if (done < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    len_left -= done;
    data_left += done;
  }
  return 0;
}

int net_send_u32(struct net_socket *sock, uint32_t data)
{
  unsigned char bytes[4];
  pack_u32(bytes, 0, data);
  return net_send_data(sock, bytes, sizeof(bytes));
}

int net_recv_data(struct net_socket *sock, void *data, size_t len)
{
  size_t len_left = len;
  char *data_left = data;
  while (len_left > 0) {
    int done = recv(sock->sock, data_left, (int) len_left, 0);
    if (done < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    len_left -= done;
    data_left += done;
  }
  return 0;
}

int net_recv_u32(struct net_socket *sock, uint32_t *data)
{
  unsigned char bytes[4];
  if (net_recv_data(sock, bytes, sizeof(bytes)) < 0) {
    return -1;
  }
  *data = unpack_u32(bytes, 0);
  return 0;
}

uint32_t net_get_beacon_message_id(struct net_msg_beacon *beacon)
{
  return beacon->message_id;
}

static struct net_socket *make_net_socket(sock_type sock)
{
  struct net_socket *net_socket = malloc(sizeof(*net_socket));
  if (net_socket != NULL) {
    DebugLog("========== ALLOCATED SOCKET %p\n", net_socket);
    net_socket->sock = sock;
  }
  return net_socket;
}

struct net_msg_beacon *make_net_beacon(unsigned char *data, size_t data_len, struct sockaddr *addr)
{
  if (data_len < BEACON_PACKET_SIZE) {
    DebugLog("ERROR: beacon data is too small (%d bytes)\n", (int) data_len);
    return NULL;
  }
  uint32_t beacon_magic = unpack_u32(data, 0);
  uint32_t beacon_version = unpack_u32(data, 4);
  uint32_t net_port = unpack_u32(data, 8);
  uint32_t client_id = unpack_u32(data, 12);
  uint32_t message_id = unpack_u32(data, 16);

  if (beacon_magic != BEACON_MAGIC) {
    DebugLog("ERROR: invalid beacon magic: 0x%04x\n", beacon_magic);
    return NULL;
  }
  if (beacon_version != BEACON_VERSION) {
    DebugLog("ERROR: invalid beacon version: 0x%04x\n", beacon_version);
    return NULL;
  }

  struct net_msg_beacon *beacon = malloc(sizeof(*beacon));
  if (beacon == NULL) {
    DebugLog("ERROR: out of memory for handling message\n");
    return NULL;
  }
  DebugLog("======================= ALLOCATED BACON %p\n", beacon);

  beacon->net_family = addr->sa_family;
  get_address_host(addr, beacon->net_host, sizeof(beacon->net_host));
  snprintf(beacon->net_port, sizeof(beacon->net_port), "%u", net_port);
  beacon->client_id = client_id;
  beacon->message_id = message_id;
  return beacon;
}

int net_udp_server(net_beacon_callback callback, void *user_data)
{
  char listen_port[16];
  snprintf(listen_port, sizeof(listen_port), "%d", config.udp_server_port);

  sock_type sock = open_server_socket(SOCK_DGRAM, listen_port, config.use_ipv6, NULL, NULL);
  if (sock < 0) {
    DebugLog("ERROR: can't open UDP server socket\n");
    return -1;
  }

  while (1) {
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    unsigned char data[BEACON_PACKET_MAX_SIZE];
    int data_len = recvfrom(sock, data, sizeof(data), 0, (struct sockaddr *) &addr, &addr_len);
    if (data_len < 0) {
      if (errno == EINTR) {
        continue;
      }
      DebugLog("ERROR: recvfrom returns %d, errno is %d\n", data_len, errno);
      close(sock);
      return -2;
    }

    struct net_msg_beacon *beacon = make_net_beacon(data, data_len, (struct sockaddr *) &addr);
    if (beacon != NULL && callback(beacon, user_data) != 0) {
      break;
    }
  }

  close(sock);
  return 0;
}

int net_tcp_server(net_connect_callback callback, void *user_data)
{
  char listen_port[16];
  snprintf(listen_port, sizeof(listen_port), "%d", config.tcp_server_port);

  sock_type server_sock = open_server_socket(SOCK_STREAM, listen_port, config.use_ipv6, NULL, NULL);
  if (server_sock < 0) {
    DebugLog("ERROR: can't open TCP server socket\n");
    return -1;
  }

  if (listen(server_sock, TCP_BACKLOG) != 0) {
    DebugLog("ERROR: can't listen on TCP server port\n");
    close(server_sock);
    return -1;
  }

  int error_code = -1;
  while (1) {
    // handle connection
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    sock_type client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_sock < 0) {
      DebugLog("[net_tcp_server] error in accept\n");
      continue;
    }
    struct net_socket *net_socket = make_net_socket(client_sock);
    if (net_socket == NULL) {
      DebugLog("[net_tcp_server] error making socket\n");
      close(client_sock);
      continue;
    }
    DebugLog("[net_tcp_server] running callback\n");
    callback(net_socket, user_data);
  }

  close(server_sock);
  return error_code;
}

int net_send_msg_beacon(uint32_t message_id)
{
  // broadcast UDP beacon
  unsigned char beacon_data[BEACON_PACKET_SIZE];
  pack_u32(beacon_data,  0, BEACON_MAGIC);
  pack_u32(beacon_data,  4, BEACON_VERSION);
  pack_u32(beacon_data,  8, config.tcp_server_port);
  pack_u32(beacon_data, 12, config.client_id);
  pack_u32(beacon_data, 16, message_id);

  struct sockaddr_storage addr;
  socklen_t addr_len = 0;
  sock_type sock = open_udp_broadcast_socket(config.udp_server_port, config.use_ipv6, &addr, &addr_len);
  if (sock < 0) {
    return -1;
  }

  int len = sendto(sock, beacon_data, (int) sizeof(beacon_data), 0, (struct sockaddr *) &addr, addr_len);
  close(sock);

  if (len != sizeof(beacon_data)) {
    return -2;
  }
  return 0;
}

struct net_socket *net_connect_to_beacon(struct net_msg_beacon *beacon)
{
  struct net_socket *net_socket = NULL;
  DebugLog("[net_connect_to_beacon] will connect to '%s:%s'\n", beacon->net_host, beacon->net_port);
  sock_type sock = socket(beacon->net_family, SOCK_STREAM, 0);
  if (sock < 0) goto end;

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = beacon->net_family;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo *servinfo;
  if (getaddrinfo(beacon->net_host, beacon->net_port, &hints, &servinfo) != 0) {
    goto end;
  }

  for (struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) {
    int ret = connect(sock, p->ai_addr, (socklen_t) p->ai_addrlen);
    if (ret < 0) {
      DebugLog("[net_connect_to_beacon] ERROR connecting\n");
      continue;
    }

    DebugLog("[net_connect_to_beacon] connecting succeeds\n");
    net_socket = make_net_socket(sock);
    if (!net_socket) {
      DebugLog("[net_connect_to_beacon] out of memory for new socket\n");
      close(sock);
      continue;
    }
    break;
  }

  freeaddrinfo(servinfo);

 end:
  DebugLog("======================= FREE BACON %p\n", beacon);
  free(beacon);   // mmm, free bacon...
  if (net_socket == NULL) {
    DebugLog("[net_connect_to_beacon] connection failed\n");
  }
  return net_socket;
}
