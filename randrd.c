/*	$OpenBSD: hotplugd.c,v 1.12 2010/01/10 13:20:41 grange Exp $	*/
/*
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
 * Copyright (c) 2014-2015 Fabian Raetz <fabian.raetz@gmail.com>
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

#define _LOG_TAG			"randrd"
#define _LOG_FACILITY			LOG_DAEMON
#define _LOG_OPT			(LOG_NDELAY | LOG_PID)

Display				*dpy;
const char *connstates[] = { "connected", "disconnected", "unknown" };
const char			*script = NULL;
char				*current_edidhash;
volatile sig_atomic_t		 quit = 0;
int 				 debug = 0, interval = 3;
int				 rr_event_base, rr_event_error;

char		*getedidhash(void);
char		*getedidhash1(XRRScreenResources *);
char		*getscript(void);
void		 randrd(void);
void		 exec_script(const char *, const char *, const char *,
			const char *);
void		 sigchild(int);
void		 sigquit(int);
__dead void	 usage(void);

int
main(int argc, char *argv[])
{
	const char			*errstr;
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

	current_edidhash = getedidhash();
	if (EFlag) {
		printf("%s\n", current_edidhash);
		free(current_edidhash);
		return (0);
	}

	if (script == NULL)
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
	randrd();
	syslog(LOG_INFO, "terminated");

	XCloseDisplay(dpy);
	closelog();
	return (0);
}

char *
getedidhash(void)
{
	XRRScreenResources		*resources;
	char				*edidhash;
	Window				 root;

	root = RootWindow(dpy, DefaultScreen(dpy));
	resources = XRRGetScreenResources(dpy, root);

	edidhash = getedidhash1(resources);

	XRRFreeScreenResources(resources);
	return (edidhash);
}


char *
getedidhash1(XRRScreenResources *resources)
{
	uint8_t				*edids = NULL;
	char				*edidhash;
	unsigned char			*prop;
	Atom				 edid_atom, actual_type;
	unsigned long			 edid_nitems, new_nitems = 0, nitems = 0;
	unsigned long			 bytes_after;
	int				 i, actual_format;

	edid_atom = XInternAtom(dpy, RR_PROPERTY_RANDR_EDID, 0);

	for (i = 0; i < resources->noutput; ++i) {
		XRRGetOutputProperty(dpy, resources->outputs[i], edid_atom, 0,
			100, 0, 0, AnyPropertyType, &actual_type,
			&actual_format, &edid_nitems, &bytes_after, &prop);

		if (actual_type == XA_INTEGER && actual_format == 8) {
			new_nitems += edid_nitems;
			edids = reallocarray(edids, new_nitems, sizeof(uint8_t));
			if(edids == NULL)
				err(1, "malloc");

			memcpy(edids + nitems, prop, edid_nitems);
			nitems = new_nitems;
		}

		XFree(prop);
	}

	if (edids == NULL)
		return (NULL); /* XXX */

	edidhash = RMD160Data(edids, sizeof(*edids) * nitems, NULL);
	free(edids);
	return (edidhash);
}


char *
getscript(void)
{
	const char			*home;
	static char			 scriptbuf[1024];
	int				 ret;

	if (!(home = getenv("HOME")))
		errx(1, "can't find HOME");

	ret = snprintf(scriptbuf, sizeof(scriptbuf), "%s/.randrd", home);
	if (ret < 0 || (size_t)ret >= sizeof(scriptbuf))
		errx(1, "scriptpath to long");

	return (scriptbuf);
}

void
randrd(void)
{
	XRRScreenResources		*resources;
	XRROutputInfo			*info;
	XRROutputChangeNotifyEvent	*rrocevt;
	XRRNotifyEvent			*rrevt;
	char				*new_edidhash;
	Window				 root;
	XEvent	 			 evt;

	exec_script(script, "init", "", current_edidhash);

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

			new_edidhash = getedidhash1(resources);
			if (!strcmp(current_edidhash, new_edidhash)) {
				free(new_edidhash);
				XRRFreeScreenResources(resources);
				continue;
			}

			free(current_edidhash);
			current_edidhash = new_edidhash;
			info = XRRGetOutputInfo(rrocevt->display, resources,
					rrocevt->output);

			syslog(LOG_INFO, "%s %s", info->name,
					connstates[info->connection]);
			exec_script(script, connstates[info->connection],
					info->name, new_edidhash);

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
exec_script(const char *file, const char *connstate, const char *output,
		const char *edidhash)
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
		execl(file, basename(file), connstate, output, edidhash, (char *)NULL);
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
