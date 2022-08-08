TOPDIR = .
SUBDIRS = lib src

include ${TOPDIR}/make/comm.mk
include ${TOPDIR}/user.mk

src.all: lib.all
