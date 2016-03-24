#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <osmocom/core/conv.h>
#include <osmocom/core/crcgen.h>

#include "burst_desc.h"

enum CS {CS1 = 0, CS2, CS3, CS4, MCS1_4, MCS5_6, MCS7_9};

#define DATA_BLOCK_SIZE         184
#define PARITY_SIZE             40
#define FLUSH_BITS_SIZE         4
#define PARITY_OUTPUT_SIZE (DATA_BLOCK_SIZE + PARITY_SIZE + FLUSH_BITS_SIZE)
#define DEBUG_PRINT false
#define PREFER_MCS 0
#define CONV_SIZE		(2 * PARITY_OUTPUT_SIZE)
#define MAX_MCS 5
// 5 == MCS1-4

enum {UNKNOWN = 0, BCCH = 1, CCCH = 2, SDCCH = 4, SACCH = 8};

/*
 * GSM PDTCH CS-2, CS-3, CS-4 parity
 *
 * g(x) = x^16 + x^12 + x^5 + 1
 */
static const struct osmo_crc16gen_code gsm0503_cs234_crc16 = {
	.bits = 16,
	.poly = 0x1021,
	.init = 0x0000,
	.remainder = 0xffff,
};

/*
 * GSM (SACCH) parity (FIRE code)
 *
 * g(x) = (x^23 + 1)(x^17 + x^3 + 1)
 *      = x^40 + x^26 + x^23 + x^17 + x^3 + a1
 */
static const struct osmo_crc64gen_code gsm0503_fire_crc40 = {
	.bits = 40,
	.poly = 0x0004820009ULL,
	.init = 0x0000000000ULL,
	.remainder = 0xffffffffffULL,
};

struct burst_buf {
	unsigned count;
	unsigned errors;
	unsigned snr[2 * 4];
	unsigned rxl[2 * 4];
	uint32_t fn[2 * 4];
	uint8_t data[2 * 4 * 114];
	uint8_t sbit[2 * 4 * 2];
};

struct burst_buf gprs[16];

void gprs_init();
int process_pdch(struct l1ctl_burst_ind *bi, bool print);
