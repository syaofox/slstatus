/* See LICENSE file for copyright and license details. */
#include <limits.h>
#include <stdio.h>

#include "../slstatus.h"
#include "../util.h"

#if defined(__linux__)
	#include <stdint.h>
	#include <string.h>
	#include <time.h>

	#define NET_RX_BYTES "/sys/class/net/%s/statistics/rx_bytes"
	#define NET_TX_BYTES "/sys/class/net/%s/statistics/tx_bytes"

	static const char *
	get_default_interface(void)
	{
		FILE *fp;
		static char iface[32] = {0};
		static time_t last_check = 0;
		static const unsigned int cache_interval = 10;
		extern const unsigned int interval;
		char line[128];

		if (iface[0] != '\0' && time(NULL) - last_check < (time_t)(interval * cache_interval / 1000))
			return iface;

		fp = fopen("/proc/net/route", "r");
		if (!fp)
			return NULL;

		if (fgets(line, sizeof(line), fp) == NULL) {
			fclose(fp);
			return NULL;
		}

		while (fgets(line, sizeof(line), fp)) {
			unsigned int dest, gw, flags;
			if (sscanf(line, "%31s %x %x %x", iface, &dest, &gw, &flags) >= 4) {
				if (dest == 0 && (flags & 0x02)) {
					fclose(fp);
					last_check = time(NULL);
					return iface;
				}
			}
		}
		fclose(fp);
		return NULL;
	}

	const char *
	netspeed_rx(const char *interface)
	{
		uintmax_t oldrxbytes;
		static uintmax_t rxbytes;
		extern const unsigned int interval;
		char path[PATH_MAX];

		oldrxbytes = rxbytes;

		if (esnprintf(path, sizeof(path), NET_RX_BYTES, interface) < 0)
			return NULL;
		if (pscanf(path, "%ju", &rxbytes) != 1)
			return NULL;
		if (oldrxbytes == 0)
			return NULL;

		return fmt_human((rxbytes - oldrxbytes) * 1000 / interval,
		                 1024);
	}

	const char *
	netspeed_tx(const char *interface)
	{
		uintmax_t oldtxbytes;
		static uintmax_t txbytes;
		extern const unsigned int interval;
		char path[PATH_MAX];

		oldtxbytes = txbytes;

		if (esnprintf(path, sizeof(path), NET_TX_BYTES, interface) < 0)
			return NULL;
		if (pscanf(path, "%ju", &txbytes) != 1)
			return NULL;
		if (oldtxbytes == 0)
			return NULL;

		return fmt_human((txbytes - oldtxbytes) * 1000 / interval,
		                 1024);
	}

	const char *
	netspeed_combined(const char *interface)
	{
		uintmax_t oldrxbytes, oldtxbytes;
		static uintmax_t rxbytes, txbytes;
		extern const unsigned int interval;
		char path[PATH_MAX];
		double tx_mbps, rx_mbps;

		oldrxbytes = rxbytes;
		oldtxbytes = txbytes;

		if (esnprintf(path, sizeof(path), NET_RX_BYTES, interface) < 0)
			return NULL;
		if (pscanf(path, "%ju", &rxbytes) != 1)
			return NULL;

		if (esnprintf(path, sizeof(path), NET_TX_BYTES, interface) < 0)
			return NULL;
		if (pscanf(path, "%ju", &txbytes) != 1)
			return NULL;

		if (oldrxbytes == 0 || oldtxbytes == 0)
			return NULL;

		tx_mbps = (double)(txbytes - oldtxbytes) * 8.0 * 1000.0 / interval / 1000000.0;
		rx_mbps = (double)(rxbytes - oldrxbytes) * 8.0 * 1000.0 / interval / 1000000.0;

		return bprintf("%.02f/%.02f Mbps", tx_mbps, rx_mbps);
	}

	const char *
	netspeed_auto(const char *mode)
	{
		const char *iface = get_default_interface();
		uintmax_t oldrxbytes, oldtxbytes;
		static uintmax_t rxbytes, txbytes;
		extern const unsigned int interval;
		char path[PATH_MAX], tx_str[16], rx_str[16];
		uintmax_t tx_diff, rx_diff;

		if (!iface)
			return NULL;

		oldrxbytes = rxbytes;
		oldtxbytes = txbytes;

		if (esnprintf(path, sizeof(path), NET_RX_BYTES, iface) < 0)
			return NULL;
		if (pscanf(path, "%ju", &rxbytes) != 1)
			return NULL;

		if (esnprintf(path, sizeof(path), NET_TX_BYTES, iface) < 0)
			return NULL;
		if (pscanf(path, "%ju", &txbytes) != 1)
			return NULL;

		if (oldrxbytes == 0 || oldtxbytes == 0)
			return NULL;

		tx_diff = txbytes - oldtxbytes;
		rx_diff = rxbytes - oldrxbytes;

		if (mode == NULL || mode[0] == '\0' || !strcmp(mode, "Mbps"))
			return bprintf("%.02f/%.02f Mbps",
			       (double)tx_diff * 8.0 * 1000.0 / interval / 1000000.0,
			       (double)rx_diff * 8.0 * 1000.0 / interval / 1000000.0);
		if (!strcmp(mode, "MB/s"))
			return bprintf("%.02f/%.02f MB/s",
			       (double)tx_diff * 1000.0 / interval / 1000000.0,
			       (double)rx_diff * 1000.0 / interval / 1000000.0);

		esnprintf(tx_str, sizeof(tx_str), "%s",
		          fmt_human(tx_diff * 1000 / interval, 1024));
		esnprintf(rx_str, sizeof(rx_str), "%s",
		          fmt_human(rx_diff * 1000 / interval, 1024));
		return bprintf("%s/%s", tx_str, rx_str);
	}
#elif defined(__OpenBSD__) | defined(__FreeBSD__)
	#include <ifaddrs.h>
	#include <net/if.h>
	#include <string.h>
	#include <sys/types.h>
	#include <sys/socket.h>

	const char *
	netspeed_rx(const char *interface)
	{
		struct ifaddrs *ifal, *ifa;
		struct if_data *ifd;
		uintmax_t oldrxbytes;
		static uintmax_t rxbytes;
		extern const unsigned int interval;
		int if_ok = 0;

		oldrxbytes = rxbytes;

		if (getifaddrs(&ifal) < 0) {
			warn("getifaddrs failed");
			return NULL;
		}
		rxbytes = 0;
		for (ifa = ifal; ifa; ifa = ifa->ifa_next)
			if (!strcmp(ifa->ifa_name, interface) &&
			   (ifd = (struct if_data *)ifa->ifa_data))
				rxbytes += ifd->ifi_ibytes, if_ok = 1;

		freeifaddrs(ifal);
		if (!if_ok) {
			warn("reading 'if_data' failed");
			return NULL;
		}
		if (oldrxbytes == 0)
			return NULL;

		return fmt_human((rxbytes - oldrxbytes) * 1000 / interval,
		                 1024);
	}

	const char *
	netspeed_tx(const char *interface)
	{
		struct ifaddrs *ifal, *ifa;
		struct if_data *ifd;
		uintmax_t oldtxbytes;
		static uintmax_t txbytes;
		extern const unsigned int interval;
		int if_ok = 0;

		oldtxbytes = txbytes;

		if (getifaddrs(&ifal) < 0) {
			warn("getifaddrs failed");
			return NULL;
		}
		txbytes = 0;
		for (ifa = ifal; ifa; ifa = ifa->ifa_next)
			if (!strcmp(ifa->ifa_name, interface) &&
			   (ifd = (struct if_data *)ifa->ifa_data))
				txbytes += ifd->ifi_obytes, if_ok = 1;

		freeifaddrs(ifal);
		if (!if_ok) {
			warn("reading 'if_data' failed");
			return NULL;
		}
		if (oldtxbytes == 0)
			return NULL;

		return fmt_human((txbytes - oldtxbytes) * 1000 / interval,
		                 1024);
	}
#endif
