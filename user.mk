CC_boot = gcc
CC_managed = gcc -B ${TOPDIR}/wrapper --no-integrated-cpp
CC = ${CC_${VARIANT}}
