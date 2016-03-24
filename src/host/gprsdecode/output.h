#pragma once

#include <stdint.h>

void net_init();
void net_send_rlcmac(uint8_t *msg, int len, uint8_t ts, uint8_t ul);
void net_send_llc(uint8_t *data, int len, uint8_t ul);
