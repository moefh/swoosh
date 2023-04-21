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

struct net_config;
struct net_socket;
struct net_msg_beacon;

typedef int (*net_msg_callback)(struct net_msg_beacon *beacon, void *user_data);
typedef void (*net_connect_callback)(struct net_socket *sock, void *user_data);

// setup network
int net_setup(uint32_t client_id, int server_udp_port, int first_tcp_port, int last_tcp_port, int use_ipv6);

// listen to UDP broadcast beacons and run the callback for each beacon received
int net_udp_server(net_msg_callback callback, void *user_data);

// connect to server to receive a message
struct net_socket *net_connect_to_beacon(struct net_msg_beacon *address);

// broadcast an UDP beacon, wait for TCP connections and run the callback for each TCP connection received
int net_msg_send(net_connect_callback callback, void *user_data);

// send data to a socket
int net_send_data(struct net_socket *sock, const void *data, size_t len);

// receive data from a socket
int net_recv_data_len(struct net_socket *sock, uint32_t *data_len);
int net_recv_data(struct net_socket *sock, void *data, size_t len);

// close a socket
void net_close_socket(struct net_socket *sock);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_H_FILE */
