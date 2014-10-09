PROG =		monplugd
NOMAN =		NOMAN

LDADD +=	-L/usr/X11R6/lib
LDADD +=	-lX11 -lXrandr
CFLAGS +=	-I/usr/X11R6/include -Wall
CFLAGS +=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS +=	-Wmissing-declarations
CFLAGS +=	-Wshadow -Wpointer-arith
CFLAGS +=	-Wsign-compare

DEBUG =		-g

.include <bsd.prog.mk>
