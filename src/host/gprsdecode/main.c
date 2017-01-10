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

#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>


char *pipe_name = "/tmp/gprs_fifo";
char *file_name = "";
int read_from_file = 0;			// default use pipe to read data

const char *openbsc_copyright =
	"License GPLv2+: GNU GPL version 2 or later "
		"<http://gnu.org/licenses/gpl.html>\n"
	"This is free software: you are free to change and redistribute it.\n"
	"There is NO WARRANTY, to the extent permitted by law.\n\n";

const char *author = "Modify by ddvv <dadavivi512@gmail.com> at 2017.01.01\n\n";

static void print_usage(const char *app)
{
	printf("Usage: %s\n", app);
}

static void print_help()
{
	printf(" Some help...\n");
	printf("  -h --help		this text.\n");
	printf("  -v --version		Show the current version.\n");
	printf("  -p --pipe		Path to the pipe. Default pipe is /tmp/gprs_fifo\n");
	printf("  -f --file		Path to the file. \n");

}

static void print_version()
{
	printf("Version is 1.0.1\n");
}

static void set_config(char **opt, struct option **option)
{
	static struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'v'},
		{"pipe", 1, 0, 'p'},
		{"file", 1, 0, 'f'},
		{0,0,0,0}
	};


	*opt = "hvp:f:";
	*option = long_options;
}

static void handle_options(int argc, char **argv)
{
	struct option *long_options;
	char *opt;

	set_config(&opt, &long_options);

	while (1) {
		int option_index = 0, c;

		c = getopt_long(argc, argv, opt, long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_usage(argv[0]);
			print_help();
			exit(0);
			break;
		case 'v':
			print_version();
			exit(0);
			break;
		case 'p':
			pipe_name = optarg;
			printf("pipe name is : %s\n", pipe_name);
			break;
		case 'f':
			file_name = optarg;
			read_from_file = 1;
			printf("file name is :%s\n", file_name);
			break;
		default:
			break;
		}
	}
}

static void print_copyright()
{
	printf("%s", openbsc_copyright);
	printf("%s", author);
}

void process_handle_burst(struct l1ctl_burst_ind *bi)
{
	uint32_t fn;
	uint8_t type, subch, ts;

	fn = ntohl(bi->frame_nr);
	rsl_dec_chan_nr(bi->chan_nr, &type, &subch, &ts);

	switch (type) {
	case RSL_CHAN_Bm_ACCHs:
	    if ((ts > 0) && ((fn % 13) != 12))
		process_pdch(bi, DEBUG_PRINT);
		break;
	case RSL_CHAN_Lm_ACCHs:
	case RSL_CHAN_BCCH:
	case RSL_CHAN_SDCCH4_ACCH:
	case RSL_CHAN_SDCCH8_ACCH:
	case RSL_CHAN_RACH:
	case RSL_CHAN_PCH_AGCH:
	default:
		break;
	}
}

void decode_pipe(const char *pipe_name)
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

void decode_file(char *file_name)
{
	int ret;
	FILE *burst_fd;
    struct l1ctl_burst_ind bi = {0};

	burst_fd = fopen(file_name, "rb");
	if (!burst_fd) {
		printf("Cannot open file\n");
		exit(0);
	}

	while (!feof(burst_fd)) {
		ret = fread(&bi, sizeof(bi), 1, burst_fd);
		if (!ret)
			break;
		process_handle_burst(&bi);
	}

}

int main(int argc, char **argv)
{
	print_copyright();
	handle_options(argc, argv);

	net_init();
	gprs_init();
	memset(gprs, 0, 16 * sizeof(struct burst_buf));

	if(read_from_file)
		decode_file(file_name);
	else
		decode_pipe(pipe_name);
	fflush(NULL);
	return 0;
}

