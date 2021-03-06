#include <string.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "lwipthread.h"
#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include <lwip/tcpip.h>
#include "netif/etharp.h"
#include "netif/ppp_oe.h"
#include <lwip/sockets.h>

#include "valve_controller.h"
#include "discovery.h"

#define ID_BASE 0x1ffff7e8

struct valve_controller vc;
Discovery discovery;

static WORKING_AREA(waThread1, 128);
static msg_t Thread1(void *arg) {

	(void)arg;
	chRegSetThreadName("led");

	while (1) {
		if (vc_is_stalled(&vc)) {
			palClearPad(GPIOB, 8);
		} else {
			palTogglePad(GPIOB, 8);
		}
		chThdSleepMilliseconds(500);
	}
	return 0;
}


int main(void) {

	halInit();
	chSysInit();

	chThdSleepMilliseconds(100);

	/* Status LED thread .*/
	chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

	/* TCP/IP stack initialization. */
	uint8_t mac_address[6] = {
		0x02,
		*(unsigned char *)(ID_BASE + 7),
		*(unsigned char *)(ID_BASE + 8),
		*(unsigned char *)(ID_BASE + 9),
		*(unsigned char *)(ID_BASE + 10),
		*(unsigned char *)(ID_BASE + 11)
	};
	struct lwipthread_opts default_ip = {
		.macaddress = mac_address,
		.address = 0x1400a8c0,        /* 192.168.0.20  */
		.netmask = 0x00ffffff,        /* 255.255.255.0 */
		.gateway = 0x0100a8c0         /* 192.168.0.1   */
	};
	chThdCreateStatic(wa_lwip_thread, LWIP_THREAD_STACK_SIZE, NORMALPRIO + 1, lwip_thread, &default_ip);

	/* Valve controller initialization. */
	vc_init(&vc);

	discovery_init(&discovery);
	discovery_set_mac(&discovery, mac_address);

	/* Main task observing link cnages and doing DHCP. */
	int last_link_status = 0;
	while (1) {

		int current_link_status = netif_is_link_up(netif_default);
		if (current_link_status != last_link_status) {
			if (current_link_status) {
				netifapi_dhcp_start(netif_default);
			} else {
				netifapi_dhcp_stop(netif_default);
			}
		}
		last_link_status = current_link_status;

		/* GPIOC0 is orange ethernet socket LED. Lit it when DHCP is bound. */
		if (netif_default->dhcp->state == DHCP_BOUND) {
			palClearPad(GPIOC, 0);
			discovery_set_ip(&discovery, (uint8_t *)&(netif_default->ip_addr.addr));
			discovery_send_broadcast(&discovery);
		} else {
			palSetPad(GPIOC, 0);
		}

		chThdSleepMilliseconds(100);
	}
}
