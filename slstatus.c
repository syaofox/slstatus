/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>

#include "arg.h"
#include "slstatus.h"
#include "util.h"

struct arg {
	const char *(*func)(const char *);
	const char *fmt;
	const char *args;
	unsigned int refresh_ms;
};

char buf[1024];
static volatile sig_atomic_t done;
static Display *dpy;

#include "config.h"

struct argstate {
	struct timespec next_update;
	unsigned int refresh_ms;
	int initialized;
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

	do {
		if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
			die("clock_gettime:");

		status[0] = '\0';
		for (i = len = 0; i < LEN(args); i++) {
			if (!states[i].initialized ||
			    timespec_ge(&start, &states[i].next_update)) {
				if (!(res = args[i].func(args[i].args)))
					res = unknown_str;

				if ((ret = esnprintf(states[i].text,
				                     sizeof(states[i].text),
				                     args[i].fmt, res)) < 0)
					break;

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

			if (wait.tv_sec >= 0 &&
			    nanosleep(&wait, NULL) < 0 &&
			    errno != EINTR)
					die("nanosleep:");
		}
	} while (!done);

	if (!sflag) {
		XStoreName(dpy, DefaultRootWindow(dpy), NULL);
		if (XCloseDisplay(dpy) < 0)
			die("XCloseDisplay: Failed to close display");
	}

	return 0;
}
