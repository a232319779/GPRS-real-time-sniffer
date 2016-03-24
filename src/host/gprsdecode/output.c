#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <osmocom/core/gsmtap.h>
#include <osmocom/core/gsmtap_util.h>

#include "burst_desc.h"
#include "output.h"

static struct gsmtap_inst *gti;

void net_init()
{
	gti = gsmtap_source_init("127.0.0.1", GSMTAP_UDP_PORT, 0);
	gsmtap_source_add_sink(gti);
}

void net_send_rlcmac(uint8_t *msg, int len, uint8_t ts, uint8_t ul)
{
	if (gti) {
		gsmtap_send(gti, ul ? GSMTAP_ARFCN_F_UPLINK : 0, ts, GSMTAP_CHANNEL_PACCH, 0, 0, 0, 0, msg, len);
		while (osmo_select_main(1));
	}
}

void net_send_llc(uint8_t *data, int len, uint8_t ul)
{
	struct msgb *msg;
	struct gsmtap_hdr *gh;
	uint8_t *dst;

	if (!gti)
		return;

	/* skip null frames */
	if ((data[0] == 0x43) &&
	    (data[1] == 0xc0) &&
	    (data[2] == 0x01))
		return;

	msg = msgb_alloc(sizeof(*gh) + len, "gsmtap_tx");
	if (!msg)
	        return;

	gh = (struct gsmtap_hdr *) msgb_put(msg, sizeof(*gh));

	gh->version = GSMTAP_VERSION;
	gh->hdr_len = sizeof(*gh) / 4;
	gh->type = GSMTAP_TYPE_GB_LLC;
	gh->timeslot = 0;
	gh->sub_slot = 0;
	gh->arfcn = ul ? htons(GSMTAP_ARFCN_F_UPLINK) : 0;
	gh->snr_db = 0;
	gh->signal_dbm = 0;
	gh->frame_number = 0;
	gh->sub_type = 0;
	gh->antenna_nr = 0;

        dst = msgb_put(msg, len);
        memcpy(dst, data, len);

	gsmtap_sendmsg(gti, msg);

	while (osmo_select_main(1));
}

