/* See LICENSE file for copyright and license details. */

/* interval between updates (in ms) */
const unsigned int interval = 1000;

/* text to show if no value can be retrieved */
static const char unknown_str[] = "n/a";

/* maximum output string length */
#define MAXLEN 2048

/*
 * function            description                     argument (example)      interval
 *
 * battery_perc        battery percentage              battery name (BAT0)     0
 *                                                     NULL on OpenBSD/FreeBSD
 * battery_remaining   battery remaining HH:MM         battery name (BAT0)     0
 *                                                     NULL on OpenBSD/FreeBSD
 * battery_state       battery charging state          battery name (BAT0)     0
 *                                                     NULL on OpenBSD/FreeBSD
 * cat                 read arbitrary file             path                    0
 * cpu_freq            cpu frequency in MHz            NULL                    0
 * cpu_perc            cpu usage in percent            NULL                    0
 * datetime            date and time                   format string (%F %T)   0
 * disk_free           free disk space in GB           mountpoint path (/)     0
 * disk_perc           disk usage in percent           mountpoint path (/)     0
 * disk_total          total disk space in GB           mountpoint path (/)     0
 * disk_used           used disk space in GB           mountpoint path (/)     0
 * entropy             available entropy               NULL                    0
 * gid                 GID of current user             NULL                    0
 * gpu_combined        GPU util, temp and VRAM usage    NULL (Linux only,       0
 *                                                     requires NVML)
 * gpu_perc            GPU utilization percent           NULL (Linux only,       0
 *                                                     requires NVML)
 * gpu_temp            GPU temperature in celsius        NULL (Linux only,       0
 *                                                     requires NVML)
 * gpu_vram            GPU VRAM usage in GB             NULL (Linux only,       0
 *                                                     requires NVML)
 * hostname            hostname                        NULL                    0
 * ipv4                IPv4 address                    interface name (eth0)   0
 * ipv6                IPv6 address                    interface name (eth0)   0
 * kernel_release      `uname -r`                      NULL                    0
 * keyboard_indicators caps/num lock indicators        format string (c?n?)   0
 *                                                     see keyboard_indicators.c
 * keymap              layout (variant) of current     NULL                    0
 *                     keymap
 * load_avg            load average                    NULL                    0
 * netspeed_auto       auto detect iface, select unit   NULL/"Mbps" (bit/s),    0
 *                                                     "MB/s" (byte/s),
 *                                                     else human (KiB/s)
 * netspeed_rx         receive network speed           interface name (wlan0) 0
 * netspeed_tx         transfer network speed          interface name (wlan0) 0
 * num_files           number of files in a directory  path                    0
 *                                                     (/home/foo/Inbox/cur)
 * ram_free            free memory in GB               NULL                    0
 * ram_perc            memory usage in percent          NULL                    0
 * ram_total           total memory size in GB          NULL                    0
 * ram_used            used memory in GB               NULL                    0
 * run_command         custom shell command            command (echo foo)     0
 * separator           separator string                separator string       0
 *                                                     (default: " | ")
 * swap_free           free swap in GB                 NULL                    0
 * swap_perc           swap usage in percent           NULL                    0
 * swap_total          total swap size in GB           NULL                    0
 * swap_used           used swap in GB                 NULL                    0
 * temp                temperature in degree celsius   sensor file             0
 *                                                     (/sys/class/thermal/...)
 *                                                     NULL on OpenBSD
 *                                                     thermal zone on FreeBSD
 *                                                     (tz0, tz1, etc.)
 * uid                 UID of current user             NULL                    0
 * up                  interface is running            interface name (eth0)   0
 * uptime              system uptime                   NULL                    0
 * username            username of current user        NULL                    0
 * vol_perc            OSS/ALSA volume in percent      mixer file (/dev/mixer) 0
 *                                                     NULL on OpenBSD/FreeBSD
 * wifi_essid          WiFi ESSID                      interface name (wlan0) 0
 * wifi_perc           WiFi signal in percent          interface name (wlan0) 0
 *
 * Xresources color support:
 *   res_name: Xresources resource name for dynamic color
 *   Usage: add res_name to args, then set in Xresources:
 *     slstatus.*.color: #RRGGBB
 */
#include "slstatus.h"

static const struct arg args[] = {
	/* function			format          				argument            interval	res_name */
	{ netspeed_auto, 		" %s",      				"MB/s",               1000,		"net" },
	{ separator,     		"%s",         				" | ",     			0,			"sep" },
	{ cpu_perc,     		"CPU %s%% ", 				NULL, 				2000,		"cpu" },
	{ temp,         		"%s°C ", 					"/sys/class/thermal/thermal_zone0/temp", 2000,	"temp" },
	{ ram_used,    			"RAM %s", 					"1000", 			2000,		"ram" },
	{ separator,     		"%s",         				" | ",     			0,			"sep" },
	{ gpu_combined, 		"%s", 						NULL, 				3000,		"gpu" },
	{ separator,     		"%s",         				" | ",     			0,			"sep" },
	{ swap_perc,     		"SWAP %s%%", 				NULL, 				2000,		"swap" },
	{ separator,     		"%s",         				" | ",     			0,			"sep" },
	{ datetime,    			"%s", 						"%m-%d %H:%M", 		5000,		"time" },
	{ separator,     		"%s",         				" | ",     			0,			"sep" },
	{ keyboard_indicators, 	"%s", 						"C?N?", 			0,			"kbd" },
	{ separator,     		"%s",         				" | ",     			0,			"sep" },
};
