TOPDIR = .
SUBDIRS = lib src

VARIANT ?= managed

include ${TOPDIR}/make/comm.mk
include ${TOPDIR}/user.mk

src.all: lib.all

boot:
	${MAKE} clean
	${MAKE} VARIANT=boot
	${MAKE} localinstall
	${MAKE} clean
	${MAKE} VARIANT=managed
	${MAKE} localinstall
	${MAKE} clean
	${MAKE} VARIANT=managed
	${MAKE} localinstall

localinstall:
	cp src/cast-pp wrapper/

export VARIANT :
.export VARIANT :
