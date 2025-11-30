PROG= backflip
NOMAN= noman

CFLAGS+= -Werror
DEBUG?= -g

.ifdef DEBUG
WARNINGS= yes
.endif

SRCS= main.c phy.c math.c csv.c
LDADD= -lm

.include <bsd.prog.mk>
