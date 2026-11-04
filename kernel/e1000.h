#ifndef BLUEAPPLE_E1000_H
#define BLUEAPPLE_E1000_H
#include <stdint.h>
int e1000_initialize(void);
void e1000_poll(void);
int e1000_link_up(void);
int e1000_send(const void* packet, uint16_t length);
int e1000_receive(uint8_t* packet, uint16_t capacity);
#endif
