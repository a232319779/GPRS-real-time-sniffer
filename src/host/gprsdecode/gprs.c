#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/bits.h>
#include <osmocom/core/gsmtap_util.h>
#include <osmocom/core/gsmtap.h>
#include <osmocom/gsm/gsm0503.h>
#include <osmocom/core/crcgen.h>

#include "crc.h"
#include "gprs.h"
#include "rlcmac.h"

void gprs_init()
{
	memset(tbf_table, 0, sizeof(tbf_table));
}

static inline unsigned distance(const uint8_t *a, const uint8_t *b, const unsigned size)
{
	int distance = 0;

	for (int i = 0; i < size; i++)
		distance += !!(a[i] ^ b[i]);

	return distance;
}

static inline const char *cs_to_s(enum CS cs)
{
    switch (cs) {
    case CS1: return "CS1";
    case CS2: return "CS2";
    case CS3: return "CS3";
    case CS4: return "CS4";
    case MCS1_4: return "MCS1_4";
    case MCS5_6: return "MCS5_6";
    case MCS7_9: return "MCS7_9";
    }
    return "LOL";
}

static inline enum CS cs_estimate(const uint8_t *sflags, bool print, unsigned prefer_mcs)
{
	if (print)
		printf("CS estimation for %s:\n", osmo_hexdump(sflags, 8));
	int i;
	unsigned cs_dist[7];
	const uint8_t cs_pattern[][8] = {{1, 1, 1, 1, 1, 1, 1, 1},     // CS1
					 {1, 1, 0, 0, 1, 0, 0, 0},    // CS2
					 {0, 0, 1, 0, 0, 0, 0, 1},   // CS3
					 {0, 0, 0, 1, 0, 1, 1, 0},  // CS4
					 {0, 0, 0, 1, 0, 1, 1, 0}, // MCS1-4
					 {0, 0, 0, 0, 0, 0, 0, 0},// MCS5-6
					 {1, 1, 1, 0, 0, 1, 1, 1}// MCS7-9
	};

	for (i = 0; i < 7; i++) {
		cs_dist[i] = distance(sflags, cs_pattern[i], 8);
		if (print)
			printf("\tD[%d (%s)] = %d\n", i, cs_to_s(i), cs_dist[i]);
	}

	if (cs_dist[0] < cs_dist[1])
		i = CS1;
	else
		i = CS2;
	if (cs_dist[2] < cs_dist[i])
		i = CS3;
	if (cs_dist[3] < cs_dist[i])
		i = CS4;

	if (prefer_mcs) {// enforce selecting MCS over CS in case of same distance
	    if (cs_dist[4] <= cs_dist[i])
		i = MCS1_4;
	} else {
	    if (cs_dist[4] < cs_dist[i])
		i = MCS1_4;
	}

	if (cs_dist[5] < cs_dist[i])
		i = MCS5_6;
	if (cs_dist[6] < cs_dist[i])
		i = MCS7_9;

	if (print)
	    printf("\tselected: %d (%s)\n\n", i, cs_to_s(i));
	return i;
}

static inline int usf6_estimate(const uint8_t *data)
{
	int i, min;
	unsigned usf_dist[8];
	const uint8_t usf_pattern[][6] = {{0, 0, 0, 0, 0, 0},
					  {0, 0, 1, 0, 1, 1},
					  {0, 1, 0, 1, 1, 0},
					  {0, 1, 1, 1, 0, 1},
					  {1, 0, 0, 1, 0, 1},
					  {1, 0, 1, 1, 1, 0},
					  {1, 1, 0, 0, 1, 1},
					  {1, 1, 1, 0, 0, 0}};


	for (i = 0; i < 8; i++)
		usf_dist[i] = distance(data, usf_pattern[i], 6);

	for (i = 1, min = 0; i < 8; i++)
		if (usf_dist[i] < usf_dist[min])
			min = i;

	return min;
}

static inline int usf12_estimate(const uint8_t *data)
{
	int i, min;
	unsigned usf_dist[8];
	const uint8_t usf_pattern[][12] = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
					   {0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1},
					   {0, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0},
					   {0, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1},
					   {1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1},
					   {1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0},
					   {1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1},
					   {1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0}};


	for (i = 0; i < 8; i++)
		usf_dist[i] = distance(data, usf_pattern[i], 12);

	for (i = 1, min = 0; i < 8; i++)
		if (usf_dist[i] < usf_dist[min])
			min = i;

	return min;
}

static inline void gsm0503_xcch_deinterleave(sbit_t *cB, sbit_t *iB)
{
	int j, B;

	for (int k = 0; k < 456; k++) {
		B = k & 3;
		j = 2 * ((49 * k) % 57) + ((k & 7) >> 2);
		cB[k] = iB[B * 114 + j];
	}
}

static inline int decode_signalling(const uint8_t *conv_data, uint8_t *msg)
{
	uint8_t decoded_data[PARITY_OUTPUT_SIZE];
	int8_t soft_input[CONV_SIZE];

	/* convert to soft bits */
	osmo_ubit2sbit(soft_input, conv_data, CONV_SIZE);

	/* Viterbi decoding */
	osmo_conv_decode(&gsm0503_xcch, soft_input, decoded_data);

	/* parity check: if error detected try to fix it */
	int ret = osmo_crc64gen_check_bits(&gsm0503_fire_crc40, decoded_data, 184, decoded_data + 184);
	if (ret) {
		FC_CTX fc_ctx;
		FC_init(&fc_ctx, PARITY_SIZE, DATA_BLOCK_SIZE);
/**/
		unsigned char crc_result[DATA_BLOCK_SIZE + PARITY_SIZE];
		ret = FC_check_crc(&fc_ctx, decoded_data, crc_result);
		if (!ret)
			return 0;
		/*
		ubit_t crc_result[DATA_BLOCK_SIZE + PARITY_SIZE];
		osmo_crc64gen_set_bits(&gsm0503_fire_crc40, decoded_data, 184, crc_result);
		*/
		memcpy(decoded_data, crc_result, sizeof crc_result);
	}

	osmo_ubit2pbit_ext(msg, 0, decoded_data, 0, DATA_BLOCK_SIZE, 1);

	return 23;
}

int process_pdch(struct l1ctl_burst_ind *bi, bool print)
{
	int len, ret, usf;
	uint32_t fn;
	uint16_t arfcn;
	struct burst_buf *bb;
	struct gprs_message gm;
	uint8_t ts, ul, conv_data[CONV_SIZE], decoded_data[2 * CONV_SIZE], gprs_msg[54];
	int8_t depunct_data[2 * CONV_SIZE];

	/* get burst parameters */
	fn = ntohl(bi->frame_nr);
	arfcn = ntohs(bi->band_arfcn);
	ul = !!(arfcn & GSMTAP_ARFCN_F_UPLINK);
	ts = bi->chan_nr & 7;

	/* select frame queue */
	bb = (ul) ? (&gprs[2 * ts + 0]) : (&gprs[2 * ts + 1]);

	/* align to first frame */
	if ((bb->count == 0) && (((fn % 13) % 4) != 0))
	    return -1;

	/* enqueue data into message buffer */
	osmo_pbit2ubit(bb->data + bb->count * 114, bi->bits, 114);

	/* save stealing flags */
	bb->sbit[bb->count * 2 + 0] = !!(bi->bits[14] & 0x10); // check stealing bits location for MCS > 4
	bb->sbit[bb->count * 2 + 1] = !!(bi->bits[14] & 0x20);
	if (print)
		printf("added stealing bits (%d::%d) at [%d::%d]\n", !!(bi->bits[14] & 0x10), !!(bi->bits[14] & 0x20), bb->count * 2 + 0, bb->count * 2 + 1);
	bb->snr[bb->count] = bi->snr;
	bb->rxl[bb->count] = bi->rx_level;
	bb->fn[bb->count] = fn;
	bb->count++;

	/* Return if not enough bursts for a full message */
	if (bb->count < 4)
		return -2;

	/* de-interleaving */
	memset(conv_data, 0, sizeof(conv_data));
	gsm0503_xcch_deinterleave((sbit_t *)conv_data, (sbit_t *)bb->data); // FIXME: hack, working with non-soft bits
	len = 0;

	enum CS cs = cs_estimate(bb->sbit, DEBUG_PRINT, PREFER_MCS);
	switch (cs) {
	case CS1:
		if (print)
			printf("processing CS1 (%d::%d)\n", *(bb->sbit), cs);
		len = decode_signalling(conv_data, gprs_msg);
		break;
	case CS2:
		if (print)
			printf("processing CS2 (%d::%d)\n", *(bb->sbit), cs);
		/* depuncture and convert to soft bits */
		memset(depunct_data, 0, 294 * 2);
		osmo_ubit2sbit(depunct_data, conv_data, 456);

		/* Viterbi decode */
		osmo_conv_decode(&gsm0503_cs2, depunct_data, decoded_data);

		/* decode USF bits */
		usf = usf6_estimate(decoded_data);

		/* rebuild original data string for CRC check */
		decoded_data[3] = (usf >> 2) & 1;
		decoded_data[4] = (usf >> 1) & 1;
		decoded_data[5] = (usf >> 0) & 1;

		/* compute CRC-16 (CCITT) */
		ret = osmo_crc16gen_check_bits(&gsm0503_cs234_crc16, decoded_data + 3, 271, decoded_data + 3 + 271);

		if (!ret) {
			osmo_ubit2pbit_ext(gprs_msg, 0, decoded_data + 3, 0, 33 * 8, 1);
			len = 33;
		}

		break;
	case CS3:
		if (print)
			printf("processing CS3 (%d::%d)\n", *(bb->sbit), cs);
		/* depuncture and convert to soft bits */
		memset(depunct_data, 0, 338 * 2);
		osmo_ubit2sbit(depunct_data, conv_data, 456);

		/* Viterbi decode */
		osmo_conv_decode(&gsm0503_cs3, depunct_data, decoded_data);

		/* decode USF bits */
		usf = usf6_estimate(decoded_data);

		/* rebuild original data string for CRC check */
		decoded_data[3] = (usf >> 2) & 1;
		decoded_data[4] = (usf >> 1) & 1;
		decoded_data[5] = (usf >> 0) & 1;

		/* compute CRC-16 (CCITT) */
		ret = osmo_crc16gen_check_bits(&gsm0503_cs234_crc16, decoded_data + 3, 315, decoded_data + 3 + 315);
		if (!ret) {
			osmo_ubit2pbit_ext(gprs_msg, 0, decoded_data + 3, 0, 39 * 8, 1);
			len = 39;
		}
		break;
	case CS4:
		if (print)
			printf("processing CS4 (%d::%d)\n", *(bb->sbit), cs);
		/* decode USF bits */
		usf = usf12_estimate(conv_data);

		/* rebuild original data string for CRC check */
		conv_data[9] = (usf >> 2) & 1;
		conv_data[10] = (usf >> 1) & 1;
		conv_data[11] = (usf >> 0) & 1;

		/* compute CRC-16 (CCITT) */
		ret = osmo_crc16gen_check_bits(&gsm0503_cs234_crc16, conv_data + 9, 431, conv_data + 9 + 431);
		if (!ret) {
			osmo_ubit2pbit_ext(gprs_msg, 0, conv_data + 9, 0, 53 * 8, 1);
			len = 53; // last byte not used (0x2b)
		} else {// try MCS1-4?

		}
		break;
	case MCS1_4:
		if (print)
			printf("MCS1-4 is unsupported ATM\n");
		break;
	case MCS5_6:
		if (print)
			printf("MCS5-6 is unsupported ATM\n");
		break;
	case MCS7_9:
		if (print)
			printf("MCS7-9 is unsupported ATM\n");
		break;
	default:
		printf("(M)CS estimation failed for %d: %s\n", *(bb->sbit), cs_to_s(cs));
		break;
	}

	/* if a message is decoded */
	if (len) {
		unsigned s_sum, r_sum;

		/* fill gprs message struct */
		s_sum = 0;
		r_sum = 0;
		for (int i = 0; i < 4; i++) {
			s_sum += bb->snr[i];
			r_sum += bb->rxl[i];
		}
		gm.snr = s_sum / 4;
		gm.rxl = r_sum / 4;
		gm.arfcn = arfcn;
		gm.fn = fn - 3;
		gm.ts = ts;
		gm.len = len;
		memcpy(gm.msg, gprs_msg, len);

		/* call handler */
		if (0 == rlc_type_handler(&gm))
		    cs_estimate(bb->sbit, DEBUG_PRINT, PREFER_MCS); // check (M)CS selection
	}

	/* reset buffer */
	memset(bb->data, 0, sizeof(bb->data));
	bb->count = 0;

	return len;
}

