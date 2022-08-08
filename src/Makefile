TOPDIR = ..
PROG = test
CSRCS = main.c
OBJS += ${TOPDIR}/lib/libctree.a

CFLAGS = -I ${TOPDIR} -I ${TOPDIR}/include -g -O2
LDFLAGS = -ldl -lpthread

include ${TOPDIR}/make/comm.mk
include ${TOPDIR}/make/c.mk
include ${TOPDIR}/user.mk
