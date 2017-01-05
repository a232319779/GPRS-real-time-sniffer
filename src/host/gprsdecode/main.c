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

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

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

void sniff_real_time(const char *pipe_name)
{
	int res = 0;
	int pipe_rd = -1;
	struct l1ctl_burst_ind bi = {0};

	pipe_rd = access(pipe_name, R_OK);
	if(pipe_rd == -1)
	{
		res = mkfifo(pipe_name,0777);
		if(res != 0)
		{
			printf("Create fifo error!\n");
			exit(0);
		}
	}

	pipe_rd = open(pipe_name, O_RDONLY);

	if(pipe_rd == -1)
	{
		printf("Open fifo error!\n");
		exit(0);
	}

	int times = 0;
	while(1)
	{
		res = read(pipe_rd, &bi, sizeof(bi));
		process_handle_burst(&bi);
		if(res < 0)
		{
		  fprintf(stderr,"break %d times.\n", times++);
		}
	}

	close(pipe_rd);
}

int main(int argc, char **argv)
{
	int ret;
	FILE *burst_fd;
    struct l1ctl_burst_ind bi;

	net_init();
	gprs_init();
	memset(gprs, 0, 16 * sizeof(struct burst_buf));

	// use default fifo pipe /tmp/gprs_fifo
	if (argc < 2) 
	{
		sniff_real_time("/tmp/gprs_fifo");
	}

	// use user define pipe
	else if(argc == 2)
	{
		sniff_real_time(argv[1]);
	}

	// user dat file 
	else
	{
		burst_fd = fopen(argv[1], "rb");
		if (!burst_fd) {
			printf("Cannot open file\n");
			return 0;
		}

		while (!feof(burst_fd)) {
			ret = fread(&bi, sizeof(bi), 1, burst_fd);
			if (!ret)
				break;
			process_handle_burst(&bi);
		}

		fclose(burst_fd);
	}
	fflush(NULL);
	return 0;
}

