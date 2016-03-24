#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <osmocom/gsm/rsl.h>
#include <osmocom/core/select.h>
#include <osmocom/gsm/gsm_utils.h>
#include <osmocom/gsm/protocol/gsm_04_08.h>

#include "burst_desc.h"
#include "gprs.h"
#include "output.h"

void process_handle_burst(struct l1ctl_burst_ind *bi)
{
	uint32_t fn;
	uint8_t type, subch, ts;

	fn = ntohl(bi->frame_nr);
	rsl_dec_chan_nr(bi->chan_nr, &type, &subch, &ts);

	switch (type) {
	case RSL_CHAN_Bm_ACCHs:
	    if ((ts > 0) && ((fn % 13) != 12)) // wtf is 12?
		process_pdch(bi, DEBUG_PRINT);
/*		    printf("TS = %d, FN = %d (%d)\n", ts, fn, process_pdch(bi));
		else
		    printf("TS = %d, FN = %d [%d]\n", ts, fn, fn % 13);
*/		break;
	case RSL_CHAN_Lm_ACCHs:
	case RSL_CHAN_BCCH:
	case RSL_CHAN_SDCCH4_ACCH:
	case RSL_CHAN_SDCCH8_ACCH:
	case RSL_CHAN_RACH:
	case RSL_CHAN_PCH_AGCH:
	default:
		break;
		//printf("Type not handled! %.02x\n", type);
	}
}

int main(int argc, char **argv)
{
	int ret;
	FILE *burst_fd;
        struct l1ctl_burst_ind bi;

	if (argc < 2) {
		printf("\nUsage: %s <burstfile>\n", argv[0]);
		return -1;
	}

	burst_fd = fopen(argv[1], "rb");
	if (!burst_fd) {
		printf("Cannot open file\n");
		return 0;
	}

	net_init();
	gprs_init();
	memset(gprs, 0, 16 * sizeof(struct burst_buf));

	while (!feof(burst_fd)) {
		ret = fread(&bi, sizeof(bi), 1, burst_fd);
		if (!ret)
			break;
		process_handle_burst(&bi);
	}

	fclose(burst_fd);
	fflush(NULL);
	return 0;
}

