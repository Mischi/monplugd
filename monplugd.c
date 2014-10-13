/*	$OpenBSD: hotplugd.c,v 1.12 2010/01/10 13:20:41 grange Exp $	*/
/*
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
 * Copyright (c) 2014 Fabian Raetz <fabian.raetz@gmail.com>
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

#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <rmd160.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#define _LOG_TAG			"monplugd"
#define _LOG_FACILITY			LOG_DAEMON
#define _LOG_OPT			(LOG_NDELAY | LOG_PID)

Display				*dpy;
volatile sig_atomic_t		 quit = 0;
const char			*script = NULL;
const char *connstates[] = { "connected", "disconnected", "unknown" };
int 				 debug = 0, interval = 1;
int				 rr_event_base, rr_event_error;

char		*getedidhash(void);
char		*getscript(void);
void		 monplugd(void);
void		 exec_script(const char *, const char *, char *);
void		 sigchild(int);
void		 sigquit(int);
__dead void	 usage(void);

int
main(int argc, char *argv[])
{
	const char			*errstr;
	char				*edidhash;
	struct sigaction		 sact;
	int				 ch, EFlag = 0;

	while ((ch = getopt(argc, argv, "dEf:i:")) != -1)
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'E':
			EFlag = 1;
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
	if (argc > 0 || (EFlag && optind > 2))
		usage();

	if ((dpy = XOpenDisplay(NULL)) == NULL)
		errx(1, "can't open display");

	if (!XRRQueryExtension(dpy, &rr_event_base, &rr_event_error))
		errx(1, "randr extension not available");

	if (EFlag) {
		edidhash = getedidhash();
		printf("0x%s\n", edidhash);
		free(edidhash);
		return (0);
	}

	if (!script)
		script = getscript();

	memset(&sact, 0, sizeof(sact));
	sigemptyset(&sact.sa_mask);
	sact.sa_flags = 0;
	sact.sa_handler = sigquit;
	sigaction(SIGINT, &sact, NULL);
	sigaction(SIGQUIT, &sact, NULL);
	sigaction(SIGTERM, &sact, NULL);
	sact.sa_handler = SIG_IGN;
	sigaction(SIGHUP, &sact, NULL);
	sact.sa_handler = sigchild;
	sact.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &sact, NULL);


	openlog(_LOG_TAG, _LOG_OPT, _LOG_FACILITY);
	if (!debug && daemon(0, 0) == -1)
		err(1, "daemon");

	syslog(LOG_INFO, "started");
	monplugd();
	syslog(LOG_INFO, "terminated");

	XCloseDisplay(dpy);
	closelog();
	return (0);
}

char *
getedidhash(void)
{
	XRRScreenResources		*resources;
	uint8_t				*result = NULL;
	unsigned char			*prop;
	Window				 root;
	Atom				 edid_atom, actual_type;
	unsigned long			 edid_nitems, new_nitems = 0, nitems = 0;
	unsigned long			 bytes_after;
	int				 i, actual_format;

	root = RootWindow(dpy, DefaultScreen(dpy));
	resources = XRRGetScreenResources(dpy, root);
	edid_atom = XInternAtom(dpy, RR_PROPERTY_RANDR_EDID, 0);

	for (i = 0; i < resources->noutput; ++i) {
		XRRGetOutputProperty(dpy, resources->outputs[i], edid_atom, 0,
			100, 0, 0, AnyPropertyType, &actual_type,
			&actual_format, &edid_nitems, &bytes_after, &prop);

		if (actual_type == XA_INTEGER && actual_format == 8) {
			new_nitems += edid_nitems;
			result = reallocarray(result, new_nitems, sizeof(uint8_t));
			if(result == NULL)
				err(1, "malloc");

			memcpy(result + nitems, prop, edid_nitems);
			nitems = new_nitems;
		}

		XFree(prop);
	}

	XRRFreeScreenResources(resources);
	return RMD160Data(result, sizeof(*result) * nitems, NULL);
}

char *
getscript(void)
{
	const char			*home;
	static char			 scriptbuf[1024];
	int				 ret;

	if (!(home = getenv("HOME")))
		errx(1, "can't find HOME");

	ret = snprintf(scriptbuf, sizeof(scriptbuf), "%s/.monplugd", home);
	if (ret == -1 || ret >= sizeof(scriptbuf))
		errx(1, "scriptpath to long");

	return scriptbuf;
}

void
monplugd(void)
{
	XRRScreenResources		*resources;
	XRROutputInfo			*info;
	XRROutputChangeNotifyEvent	*rrocevt;
	XRRNotifyEvent			*rrevt;
	Window				 root;
	XEvent	 			 evt;


	XRRSelectInput(dpy, DefaultRootWindow(dpy), RROutputChangeNotifyMask);

	while (!quit) {
		while (XPending(dpy)) {
			XNextEvent(dpy, &evt);

			if (evt.type != rr_event_base + RRNotify)
				continue;

			rrevt = (XRRNotifyEvent *)&evt;
			if (rrevt->subtype != RRNotify_OutputChange) 
				continue;

			rrocevt = (XRROutputChangeNotifyEvent *)&evt;
			resources = XRRGetScreenResourcesCurrent(dpy,
					rrocevt->window);
			info = XRRGetOutputInfo(rrocevt->display, resources,
					rrocevt->output);

			syslog(LOG_INFO, "%s connection state: %s",
					info->name,
					connstates[info->connection]);
			exec_script(script, connstates[info->connection],
					"EDID");

			XRRFreeOutputInfo(info);
			XRRFreeScreenResources(resources);
		}

		/*
		 * On OpenBSD, the XServer isn't notified when a display is
		 * plugged in/out.  On Linux, the XServer uses libudev to receive
		 * these notifications which is not available on OpenBSD.
		 */
		sleep(interval);

		root = RootWindow(dpy, DefaultScreen(dpy));
		resources = XRRGetScreenResources(dpy, root);
		XRRFreeScreenResources(resources);
	}
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

	fprintf(stderr, "usage:"
		"\t%s [-d] [-f file] [-i interval]\n"
		"\t%s -E\n", __progname, __progname);
	exit(1);
}
