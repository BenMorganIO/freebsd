# $FreeBSD$
.include <src.opts.mk>

_LIBARCHIVEDIR=	${.CURDIR}/../../contrib/libarchive

PROG=	bsdtar
BSDTAR_VERSION_STRING=	3.2.0

.PATH: ${_LIBARCHIVEDIR}/tar
SRCS=	bsdtar.c	\
	cmdline.c	\
	creation_set.c	\
	read.c		\
	subst.c		\
	util.c		\
	write.c

.PATH: ${_LIBARCHIVEDIR}/libarchive_fe
SRCS+=	err.c		\
	line_reader.c	\
	passphrase.c

LIBADD=	archive

.if ${MK_ICONV} != "no"
CFLAGS+=	-DHAVE_ICONV=1 -DHAVE_ICONV_H=1 -DICONV_CONST=const
.endif

CFLAGS+=	-DBSDTAR_VERSION_STRING=\"${BSDTAR_VERSION_STRING}\"
CFLAGS+=	-DPLATFORM_CONFIG_H=\"${.CURDIR}/../../lib/libarchive/config_freebsd.h\"
CFLAGS+=	-I${_LIBARCHIVEDIR}/tar -I${_LIBARCHIVEDIR}/libarchive
CFLAGS+=	-I${_LIBARCHIVEDIR}/libarchive_fe
SYMLINKS=	bsdtar ${BINDIR}/tar
MLINKS=	bsdtar.1 tar.1

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
