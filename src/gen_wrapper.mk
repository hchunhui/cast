TOPDIR = ..
PROG = gen_wrapper
CSRCS = gen_wrapper.c
OBJS += ${TOPDIR}/lib/libcast.a

CFLAGS = -I ${TOPDIR} -I ${TOPDIR}/include -Os

include ${TOPDIR}/make/comm.mk
include ${TOPDIR}/make/c.mk
include ${TOPDIR}/user.mk
