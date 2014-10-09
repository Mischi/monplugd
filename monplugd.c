/*	$OpenBSD: hotplugd.c,v 1.12 2010/01/10 13:20:41 grange Exp $	*/
/*
 * Copyright (c) 2014 Fabian Raetz <fabian.raetz@gmail.com>
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#define _LOG_TAG			"monplugd"
#define _LOG_FACILITY			LOG_DAEMON
#define _LOG_OPT			(LOG_NDELAY | LOG_PID)

volatile sig_atomic_t quit = 0;
const char *connstates[] = { "connected", "disconnected", "unknown" };

void exec_script(const char *, const char *, char *);

void sigchild(int);
void sigquit(int);
__dead void usage(void);

int
main(int argc, char *argv[])
{
	Display				*dpy;
	XRRScreenResources		*resources;
	XRROutputInfo			*info;
	XRROutputChangeNotifyEvent	*ocevt;
	char				*script = "/home/mischi/.monplugd";
	const char			*errstr;
	Window				 root;
	XRRNotifyEvent			 evt;
	struct sigaction		 sact;
	int				 c, debug = 0, interval = 1, screen;

	while ((c = getopt(argc, argv, "df:i:")) != -1)
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 'f':
			script = optarg;
			break;
		case 'i':
			interval = strtonum(optarg, 1, 60, &errstr);
			if (errstr)
				errx(1, "interval is %s: %s", errstr, optarg);
			break;
		default:
			usage();
			/* NOTREACHED */
		}

	argc -= optind;
	argv += optind;
	if (argc > 0)
		usage();

	bzero(&sact, sizeof(sact));
	sigemptyset(&sact.sa_mask);
	sact.sa_flags = 0;
	sact.sa_handler = sigquit;
	/* XXX coredumps */
	/*sigaction(SIGINT, &sact, NULL);
	sigaction(SIGQUIT, &sact, NULL);
	sigaction(SIGTERM, &sact, NULL);*/
	sact.sa_handler = SIG_IGN;
	sigaction(SIGHUP, &sact, NULL);
	sact.sa_handler = sigchild;
	sact.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &sact, NULL);

	if ((dpy = XOpenDisplay(NULL)) == NULL)
		errx(1, "can't open display");

	openlog(_LOG_TAG, _LOG_OPT, _LOG_FACILITY);
	if (!debug && daemon(0, 0) == -1)
		err(1, "daemon");

	syslog(LOG_INFO, "started");

	XRRSelectInput(dpy, DefaultRootWindow(dpy), RROutputChangeNotifyMask);

	while (!quit) {
		while (XPending(dpy)) {
			XNextEvent(dpy, (XEvent *)&evt);
			resources = XRRGetScreenResources(dpy, evt.window);

			switch (evt.subtype) {
			case RRNotify_OutputChange:
				ocevt = (XRROutputChangeNotifyEvent *)&evt;
				info = XRRGetOutputInfo(evt.display, resources,
						ocevt->output);

				syslog(LOG_INFO, "%s connection state: %s",
						info->name,
						connstates[info->connection]);

				exec_script(script, connstates[info->connection],
						"EDID");

				XRRFreeOutputInfo(info);
				break;
			default:
				syslog(LOG_NOTICE, "unknown event");
				break;
			}

			XRRFreeScreenResources(resources);
		}

		/*
		 * On OpenBSD, the XServer isn't notified when a display is
		 * plugged in/out.  On Linux, the XServer uses libudev to receive
		 * these notifications which is not available on OpenBSD.
		 */
		sleep(interval);

		screen = DefaultScreen(dpy);
		root = RootWindow(dpy, screen);
		XRRGetScreenResources(dpy, root);
	}

	syslog(LOG_INFO, "terminated");
	closelog();

	return (0);
}

void
exec_script(const char *file, const char *connstate, char *name)
{
	pid_t pid;

	if (access(file, X_OK | R_OK) == -1) {
		if (errno != ENOENT)
			syslog(LOG_ERR, "%s: %m", file);
		return;
	}

	if ((pid = fork()) == -1) {
		syslog(LOG_ERR, "fork: %m");
		return;
	}
	if (pid == 0) {
		/* child process */
		execl(file, basename(file), connstate, name, (char *)NULL);
		syslog(LOG_ERR, "execl %s: %m", file);
		_exit(1);
		/* NOTREACHED */
	}
}

/* ARGSUSED */
void
sigchild(int signum)
{
	struct syslog_data sdata = SYSLOG_DATA_INIT;
	int saved_errno, status;
	pid_t pid;

	saved_errno = errno;

	sdata.log_tag = _LOG_TAG;
	sdata.log_fac = _LOG_FACILITY;
	sdata.log_stat = _LOG_OPT;

	while ((pid = waitpid(WAIT_ANY, &status, WNOHANG)) != 0) {
		if (pid == -1) {
			if (errno == EINTR)
				continue;
			if (errno != ECHILD)
				syslog_r(LOG_ERR, &sdata, "waitpid: %m");
			break;
		}

		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != 0) {
				syslog_r(LOG_NOTICE, &sdata,
				    "child exit status: %d",
				    WEXITSTATUS(status));
			}
		} else {
			syslog_r(LOG_NOTICE, &sdata,
			    "child is terminated abnormally");
		}
	}

	errno = saved_errno;
}

/* ARGSUSED */
void
sigquit(int signum)
{
	quit = 1;
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-d] [-f file] [-i interval]\n", __progname);
	exit(1);
}
