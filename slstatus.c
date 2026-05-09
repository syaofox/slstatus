/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xresource.h>

#include "arg.h"
#include "slstatus.h"
#include "util.h"

struct arg {
	const char *(*func)(const char *);
	const char *fmt;
	const char *args;
	unsigned int refresh_ms;
	const char *res_name;
};

char buf[1024];
static volatile sig_atomic_t done;
static Display *dpy;
static int xkb_available;
static int xkb_first_event;

#include "config.h"

struct argstate {
	struct timespec next_update;
	unsigned int refresh_ms;
	int initialized;
	int dirty;
	char text[MAXLEN];
};

static void
add_ms(struct timespec *ts, unsigned int msec)
{
	ts->tv_sec += msec / 1000;
	ts->tv_nsec += (msec % 1000) * 1E6;
	if (ts->tv_nsec >= 1E9) {
		ts->tv_sec++;
		ts->tv_nsec -= 1E9;
	}
}

static int
timespec_ge(const struct timespec *a, const struct timespec *b)
{
	return (a->tv_sec > b->tv_sec) ||
	       (a->tv_sec == b->tv_sec && a->tv_nsec >= b->tv_nsec);
}

static void
terminate(const int signo)
{
	if (signo != SIGUSR1)
		done = 1;
}

static void
difftimespec(struct timespec *res, struct timespec *a, struct timespec *b)
{
	res->tv_sec = a->tv_sec - b->tv_sec - (a->tv_nsec < b->tv_nsec);
	res->tv_nsec = a->tv_nsec - b->tv_nsec +
	               (a->tv_nsec < b->tv_nsec) * 1E9;
}

static void
usage(void)
{
	die("usage: %s [-v] [-s] [-1]", argv0);
}

int
main(int argc, char *argv[])
{
	struct sigaction act;
	struct timespec start, current, diff, intspec, wait;
	struct argstate states[LEN(args)];
	size_t i, len;
	int sflag, ret;
	char status[MAXLEN];
	const char *res;
	char *res_colors[LEN(args)] = {0};
	XrmDatabase db;

	sflag = 0;
	ARGBEGIN {
	case 'v':
		die("slstatus-"VERSION);
		break;
	case '1':
		done = 1;
		/* FALLTHROUGH */
	case 's':
		sflag = 1;
		break;
	default:
		usage();
	} ARGEND

	if (argc)
		usage();

	for (i = 0; i < LEN(args); i++) {
		states[i].refresh_ms = args[i].refresh_ms ? args[i].refresh_ms : interval;
		states[i].next_update.tv_sec = 0;
		states[i].next_update.tv_nsec = 0;
		states[i].initialized = 0;
		states[i].dirty = 0;
		states[i].text[0] = '\0';
	}

	memset(&act, 0, sizeof(act));
	act.sa_handler = terminate;
	sigaction(SIGINT,  &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	act.sa_flags |= SA_RESTART;
	sigaction(SIGUSR1, &act, NULL);

	if (!sflag && !(dpy = XOpenDisplay(NULL)))
		die("XOpenDisplay: Failed to open display");

	if (dpy) {
		XrmInitialize();
		char *res_man = XResourceManagerString(dpy);
		if (res_man) {
			db = XrmGetStringDatabase(res_man);
			for (i = 0; i < LEN(args); i++) {
				if (args[i].res_name) {
					char resource[256];
					char *type;
					XrmValue value;
					snprintf(resource, sizeof(resource), "slstatus.%s", args[i].res_name);
					if (XrmGetResource(db, resource, "String", &type, &value))
						res_colors[i] = strdup(value.addr);
				}
			}
			XrmDestroyDatabase(db);
		}

		xkb_available = XkbQueryExtension(dpy, NULL, &xkb_first_event,
		                                  NULL, NULL, NULL);
		if (xkb_available)
			XkbSelectEvents(dpy, XkbUseCoreKbd,
			                XkbIndicatorStateNotifyMask,
			                XkbIndicatorStateNotifyMask);
	}

	do {
		if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
			die("clock_gettime:");

		status[0] = '\0';
		for (i = len = 0; i < LEN(args); i++) {
			if (!states[i].initialized || states[i].dirty ||
			    timespec_ge(&start, &states[i].next_update)) {
				states[i].dirty = 0;
				if (!(res = args[i].func(args[i].args)))
					res = unknown_str;

			if ((ret = esnprintf(states[i].text,
			                     sizeof(states[i].text),
			                     args[i].fmt, res)) < 0)
				break;

			if (res_colors[i]) {
				char colored[MAXLEN];
				if (esnprintf(colored, sizeof(colored), "^c%s^%s^d^", res_colors[i], states[i].text) > 0)
					strncpy(states[i].text, colored, sizeof(states[i].text));
			}

			states[i].initialized = 1;
				states[i].next_update = start;
				add_ms(&states[i].next_update, states[i].refresh_ms);
			}

			if ((ret = esnprintf(status + len, sizeof(status) - len,
			                     "%s", states[i].text)) < 0)
				break;

			len += ret;
		}

		if (sflag) {
			puts(status);
			fflush(stdout);
			if (ferror(stdout))
				die("puts:");
		} else {
			if (XStoreName(dpy, DefaultRootWindow(dpy), status) < 0)
				die("XStoreName: Allocation failed");
			XFlush(dpy);
		}

		if (!done) {
			if (clock_gettime(CLOCK_MONOTONIC, &current) < 0)
				die("clock_gettime:");
			difftimespec(&diff, &current, &start);

			intspec.tv_sec = interval / 1000;
			intspec.tv_nsec = (interval % 1000) * 1E6;
			difftimespec(&wait, &intspec, &diff);

			if (wait.tv_sec >= 0) {
				if (dpy) {
					int poll_timeout = wait.tv_sec * 1000 +
					                   wait.tv_nsec / 1000000;
					struct pollfd pfd;

					pfd.fd = ConnectionNumber(dpy);
					pfd.events = POLLIN;
					if (poll(&pfd, 1, poll_timeout) < 0 &&
					    errno != EINTR)
						die("poll:");

					if (pfd.revents & POLLIN) {
						XEvent ev;

						while (XPending(dpy)) {
							XNextEvent(dpy, &ev);
							if (xkb_available &&
							    ev.type == xkb_first_event) {
								XkbAnyEvent *xkbev =
									(XkbAnyEvent *)&ev;
								if (xkbev->xkb_type ==
								    XkbIndicatorStateNotify) {
									size_t j;
									for (j = 0; j < LEN(args); j++)
										if (args[j].func ==
										    keyboard_indicators)
											states[j].dirty = 1;
								}
							}
						}
					}
				} else if (nanosleep(&wait, NULL) < 0 &&
				           errno != EINTR) {
					die("nanosleep:");
				}
			}
		}
	} while (!done);

	if (!sflag) {
		XStoreName(dpy, DefaultRootWindow(dpy), NULL);
		if (XCloseDisplay(dpy) < 0)
			die("XCloseDisplay: Failed to close display");
	}

	return 0;
}
