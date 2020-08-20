#! $FreeBSD$

.PATH: ${SRCTOP}/cddl/contrib/opensolaris/lib/libzfsbootenv/common

PACKAGE=	runtime
LIB=		zfsbootenv
SHLIB_MAJOR=	1
WARNS?=		1

LIBADD+=	zfs
LIBADD+=	nvpair

INCS=	libzfsbootenv.h
SRCS=	lzbe_device.c lzbe_util.c lzbe_pair.c

CFLAGS+= -I${.CURDIR}
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/lib/libumem
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzfs/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzfsbootenv/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzfs_core/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libnvpair
CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/head

CFLAGS+= -DNEED_SOLARIS_BOOLEAN

.include <bsd.lib.mk>
