PROG =		randrd
MAN =		randrd.1

CPPFLAGS +=	-I/usr/X11R6/include
CFLAGS +=	-Wall -Wstrict-prototypes
CFLAGS +=	-Wmissing-prototypes
CFLAGS +=	-Wmissing-declarations
CFLAGS +=	-Wshadow -Wpointer-arith
CFLAGS +=	-Wsign-compare
LDADD +=	-L/usr/X11R6/lib -lX11 -lXrandr

BINDIR ?=	/usr/local/bin

.include <bsd.prog.mk>
