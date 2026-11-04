#ifndef BLUEAPPLE_NET_H
#define BLUEAPPLE_NET_H
#include <stdint.h>
void net_initialize(void);
void net_poll(void);
int net_is_configured(void);
uint32_t net_ipv4_address(void);
int net_dns_query(const char* hostname);
int net_dns_result(uint32_t* ipv4_out);
int net_tcp_connect(uint32_t ipv4, uint16_t port);
int net_tcp_connected(void);
int net_tcp_send(const void* data, uint16_t length);
int net_tcp_read(void* buffer, uint16_t capacity);
void net_tcp_close(void);
#endif
