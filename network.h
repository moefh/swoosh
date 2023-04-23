#ifndef NETWORK_H_FILE
#define NETWORK_H_FILE

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} // prevent annoying indentation
#endif

#include <stddef.h>
#include <stdint.h>

#define NET_MAKE_MAGIC(a,b,c,d)  (\
          (((uint32_t)((d)&0xff)) << 24) |\
          (((uint32_t)((c)&0xff)) << 16) |\
          (((uint32_t)((b)&0xff)) <<  8) |\
          (((uint32_t)((a)&0xff)) <<  0) )

struct net_config;
struct net_socket;
struct net_msg_beacon;

typedef int (*net_beacon_callback)(struct net_msg_beacon *beacon, void *user_data);
typedef void (*net_connect_callback)(struct net_socket *sock, void *user_data);

// setup network
int net_setup(uint32_t client_id, int server_udp_port, int server_tcp_port, int use_ipv6);

// listen to UDP broadcast beacons and run the callback for each beacon received
int net_udp_server(net_beacon_callback callback, void *user_data);

// listen to TCP connections and run the callback for each new connections
int net_tcp_server(net_connect_callback callback, void *user_data);

// connect to server to receive a message
struct net_socket *net_connect_to_beacon(struct net_msg_beacon *address);

// broadcast an UDP message beacon
int net_send_msg_beacon(uint32_t message_id);

// send data to a socket
int net_send_u32(struct net_socket *sock, uint32_t data);
int net_send_data(struct net_socket *sock, const void *data, size_t len);

// receive data from a socket
int net_recv_u32(struct net_socket *sock, uint32_t *data_len);
int net_recv_data(struct net_socket *sock, void *data, size_t len);

// extract information from beacon
uint32_t net_get_beacon_message_id(struct net_msg_beacon *beacon);

// free a beacon
void net_free_beacon(struct net_msg_beacon *beacon);

// close a socket
void net_close_socket(struct net_socket *sock);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_H_FILE */
