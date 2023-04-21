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

#define DebugLog(...) do {} while (0)
#define DebugDumpBytes(...) do {} while (0)

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

#define MAKE_MAGIC(a,b,c,d)  (\
          (((uint32_t)((d)&0xff)) << 24) |\
          (((uint32_t)((c)&0xff)) << 16) |\
          (((uint32_t)((b)&0xff)) <<  8) |\
          (((uint32_t)((a)&0xff)) <<  0) )

#define BEACON_PACKET_MAX_SIZE   256
#define BEACON_MAGIC             MAKE_MAGIC('S', 'w', 'o', 'o')
#define BEACON_VERSION           0x00000001
#define BEACON_TYPE_TEXT         MAKE_MAGIC('T', 'e', 'x', 't')

#define TCP_BACKLOG         10
#define TCP_LISTEN_TIME_MS  5000

struct net_config {
  uint32_t client_id;
  char     server_udp_port[16];
  int      first_tcp_port;
  int      last_tcp_port;
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
  uint32_t data_type;
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

static uint64_t get_time_ms(void)
{
#if _WIN32
  struct timespec ts;
  if (timespec_get(&ts, TIME_UTC) != TIME_UTC) {
    return 0;
  }
  return ((uint64_t) ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
#else
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return ((uint64_t) ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
#endif
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

static sock_type open_udp_server_socket(const char *port, int use_ipv6, struct sockaddr_storage *addr, socklen_t *addr_len)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = (use_ipv6) ? AF_INET6 : AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
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

static sock_type open_tcp_server_socket(int port, int use_ipv6, struct sockaddr_storage *addr, socklen_t *addr_len)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = (use_ipv6) ? AF_INET6 : AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  char port_str[256];
  snprintf(port_str, sizeof(port_str), "%d", port);

  struct addrinfo *servinfo;
  if (getaddrinfo(NULL, port_str, &hints, &servinfo) != 0) {
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

static sock_type open_udp_broadcast_socket(const char *port, int use_ipv6, struct sockaddr_storage *addr, socklen_t *addr_len)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = (use_ipv6) ? AF_INET6 : AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  const char *broadcast_node = (use_ipv6) ? "FF02::1" : "255.255.255.255";

  struct addrinfo *servinfo;
  if (getaddrinfo(broadcast_node, port, &hints, &servinfo) != 0) {
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

int net_setup(uint32_t client_id, int server_udp_port, int first_tcp_port, int last_tcp_port, int use_ipv6)
{
  config.client_id = client_id;
  snprintf(config.server_udp_port, sizeof(config.server_udp_port), "%d", server_udp_port);
  config.first_tcp_port = first_tcp_port;
  config.last_tcp_port = last_tcp_port;
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

int net_recv_data_len(struct net_socket *sock, uint32_t *data_len)
{
  unsigned char data[4];
  if (net_recv_data(sock, data, 4) < 0) {
    return -1;
  }
  *data_len = unpack_u32(data, 0);
  return 0;
}

struct net_msg_beacon *make_net_beacon(unsigned char *data, size_t data_len, struct sockaddr *addr)
{
  if (data_len < 24) {
    DebugLog("ERROR: beacon data is too small (%d bytes)\n", (int) data_len);
    return NULL;
  }
  uint32_t beacon_magic = unpack_u32(data, 0);
  uint32_t beacon_version = unpack_u32(data, 4);
  uint32_t beacon_size = unpack_u32(data, 8);
  uint32_t client_id = unpack_u32(data, 12);
  uint32_t net_port = unpack_u32(data, 16);
  uint32_t data_type = unpack_u32(data, 20);

  if (beacon_magic != BEACON_MAGIC) {
    DebugLog("ERROR: invalid beacon magic: 0x%04x\n", beacon_magic);
    return NULL;
  }
  if (beacon_version != BEACON_VERSION) {
    DebugLog("ERROR: invalid beacon version: 0x%04x\n", beacon_version);
    return NULL;
  }
  if (beacon_size < 24) {
    DebugLog("ERROR: invalid beacon size: %u\n", beacon_size);
    return NULL;
  }

  struct net_msg_beacon *beacon = malloc(sizeof(*beacon));
  if (beacon == NULL) {
    DebugLog("ERROR: out of memory for handling message\n");
    return NULL;
  }
  DebugLog("============= NEW BACON %p\n", beacon);

  beacon->net_family = addr->sa_family;
  get_address_host(addr, beacon->net_host, sizeof(beacon->net_host));
  snprintf(beacon->net_port, sizeof(beacon->net_port), "%u", net_port);
  beacon->client_id = client_id;
  beacon->data_type = data_type;
  return beacon;
}

int net_udp_server(net_msg_callback callback, void *user_data)
{
  sock_type sock = open_udp_server_socket(config.server_udp_port, config.use_ipv6, NULL, NULL);
  if (sock < 0) {
    DebugLog("ERROR: can't open socket\n");
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

static int send_udp_beacon(const unsigned char *data, size_t data_size, const char *port, int use_ipv6)
{
  struct sockaddr_storage addr;
  socklen_t addr_len = 0;
  sock_type sock = open_udp_broadcast_socket(port, use_ipv6, &addr, &addr_len);
  if (sock < 0) {
    return -1;
  }

  if (data_size > BEACON_PACKET_MAX_SIZE) {
    data_size = BEACON_PACKET_MAX_SIZE;
  }
  int len = sendto(sock, data, (int) data_size, 0, (struct sockaddr *) &addr, addr_len);
  close(sock);

  if (len != data_size) {
    return -2;
  }
  return 0;
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

int net_msg_send(net_connect_callback callback, void *user_data)
{
  // open listening TCP socket
  int tcp_port = -1;
  sock_type server_sock;
  for (int port = config.first_tcp_port; port <= config.last_tcp_port; port++) {
    struct sockaddr_storage listen_addr;
    socklen_t listen_addr_len = 0;
    server_sock = open_tcp_server_socket(port, config.use_ipv6, &listen_addr, &listen_addr_len);
    if (server_sock < 0) {
      DebugLog("[net_msg_send] error opening tcp server socket\n");
      continue;
    }
    if (listen(server_sock, TCP_BACKLOG) == -1) {
      DebugLog("-> port %d fails\n", port);
      close(server_sock);
      continue;
    }
    tcp_port = get_address_port((struct sockaddr *) &listen_addr);
    DebugLog("[net_msg_send] listening on port %d\n", tcp_port);
    break;
  }

  if (tcp_port < 0) {
    DebugLog("[net_msg_send] error listening\n");
    return -1;
  }

  // broadcast UDP beacon
  unsigned char udp_data[24];
  pack_u32(udp_data,  0, BEACON_MAGIC);
  pack_u32(udp_data,  4, BEACON_VERSION);
  pack_u32(udp_data,  8, sizeof(udp_data));
  pack_u32(udp_data, 12, config.client_id);
  pack_u32(udp_data, 16, tcp_port);
  pack_u32(udp_data, 20, BEACON_TYPE_TEXT);

  int ret = send_udp_beacon(udp_data, sizeof(udp_data), config.server_udp_port, config.use_ipv6);
  if (ret < 0) {
    DebugLog("[net_msg_send] error sending udp beacon\n");
    close(server_sock);
    return -1;
  }

  // now we accept TCP connections for a while
  uint64_t start_time = get_time_ms();
  if (start_time == 0) {
    DebugLog("[net_msg_send] error reading start time\n");
    close(server_sock);
    return -1;
  }

  int error_code = -1;
  fd_set sock_set;
  while (1) {
    // calculate the amount of time we'll have to wait for
    uint64_t cur_time = get_time_ms();
    if (start_time == 0) break;
    uint64_t time_diff = cur_time - start_time;
    if (time_diff > TCP_LISTEN_TIME_MS) {
      error_code = 0;   // we're done waiting, exit successfully
      DebugLog("[net_msg_send] done waiting\n");
      break;
    }
    int time_left = (int) (TCP_LISTEN_TIME_MS - time_diff);
    struct timeval timeout = {0};
    timeout.tv_sec = time_left / 1000;
    timeout.tv_usec = (time_left % 1000) * 1000;
    DebugLog("[net_msg_send] will wait for (%ld,%ld)\n", timeout.tv_sec, timeout.tv_usec);

    // prepare sock set to wait for
    FD_ZERO(&sock_set);
    FD_SET(server_sock, &sock_set);

    // wait and check for connection
    DebugLog("[net_msg_send] select()\n");
    int ret = select((int) server_sock + 1, &sock_set, NULL, NULL, &timeout);
    DebugLog("[net_msg_send] select() returns %d\n", ret);
    if (ret < 0) {
      if (errno == EINTR) continue;
      DebugLog("[net_msg_send] error in select()\n");
      break;
    }
    if (!FD_ISSET(server_sock, &sock_set)) {
      error_code = 0; // no one connected, exit successfully
      DebugLog("[net_msg_send] got no connections after waiting\n");
      break;
    }

    DebugLog("[net_msg_send] handling connection\n");
    // handle connection
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    sock_type client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_sock < 0) {
      DebugLog("[net_msg_send] error in accept\n");
      continue;
    }
    struct net_socket *net_socket = make_net_socket(client_sock);
    if (net_socket == NULL) {
      DebugLog("[net_msg_send] error making socket\n");
      close(client_sock);
      continue;
    }
    DebugLog("[net_msg_send] running callback\n");
    callback(net_socket, user_data);
  }

  close(server_sock);
  return error_code;
}

struct net_socket *net_connect_to_beacon(struct net_msg_beacon *beacon)
{
  int success = 0;
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
    success = 1;
    break;
  }

  freeaddrinfo(servinfo);

  end:
  DebugLog("======================= FREE BACON %p\n", beacon);
  free(beacon);   // mmm, free bacon...
  if (!success) {
    DebugLog("[net_connect_to_beacon] returning ERROR\n");
    return NULL;
  }
  DebugLog("[net_connect_to_beacon] returning socket %d\n", sock);
  return make_net_socket(sock);
}
