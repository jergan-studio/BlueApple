#ifndef BLUEAPPLE_NET_H
#define BLUEAPPLE_NET_H
#include <stdint.h>
void net_initialize(void);
void net_poll(void);
int net_is_configured(void);
uint32_t net_ipv4_address(void);
#endif
