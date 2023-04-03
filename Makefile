TOPDIR = .
SUBDIRS = lib src

export VARIANT ?= managed

include ${TOPDIR}/make/comm.mk
include ${TOPDIR}/user.mk

src.all: lib.all

boot:
	make clean
	make VARIANT=boot
	make localinstall
	make clean
	make VARIANT=managed
	make localinstall

localinstall:
	cp src/cast-pp wrapper/
