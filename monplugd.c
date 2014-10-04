/*
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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/randr.h>

int
main(int argc, char *argv[])
{
	const xcb_query_extension_reply_t	*extreply;
	xcb_screen_t				*root_screen;
	xcb_connection_t			*conn;
	xcb_generic_event_t			*evt;
	xcb_randr_screen_change_notify_event_t	*sce;
	int			 		 screen, evt_base;

	if ((conn = xcb_connect(NULL, &screen)) == NULL ||
	    xcb_connection_has_error(conn))
		errx(1, "cannot open display\n");
	
	root_screen = xcb_aux_get_screen(conn, screen);

	extreply = xcb_get_extension_data(conn, &xcb_randr_id);
	if (!extreply->present)
		errx(1, "no xrandr present");

	evt_base = extreply->first_event;

	xcb_randr_select_input(conn, root_screen->root,
		       XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);

	xcb_flush(conn);
	while ((evt = xcb_wait_for_event(conn))) {
		switch (evt->response_type - evt_base) {
		case XCB_RANDR_SCREEN_CHANGE_NOTIFY:
			sce = (xcb_randr_screen_change_notify_event_t *)evt;
			printf("screen change notification\n");
			break;
		default:
			printf("unknown event %d\n", evt->response_type - evt_base);
		}

		free(evt);
	}

	return (0);
}
