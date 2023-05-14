TOPDIR = ..
PROG = cast-pp
CSRCS = main.c elim_unused.c
OBJS += ${TOPDIR}/lib/libcast.a

CFLAGS = -I ${TOPDIR} -I ${TOPDIR}/include -g -O2
LDFLAGS = -ldl -lpthread

include ${TOPDIR}/make/comm.mk
include ${TOPDIR}/make/c.mk
include ${TOPDIR}/user.mk
