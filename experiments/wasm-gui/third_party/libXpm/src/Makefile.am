# Daniel Stone disowns all copyright on this file.

lib_LTLIBRARIES=libXpm.la

AM_CPPFLAGS = -I$(top_srcdir)/include/X11/
AM_CFLAGS = $(CWARNFLAGS) $(XPM_CFLAGS)

libXpm_la_LDFLAGS = -version-number 4:11:0 -no-undefined
libXpm_la_LIBADD =  $(XPM_LIBS)

libXpm_la_SOURCES =					\
	Attrib.c					\
	CrBufFrI.c					\
	CrBufFrP.c					\
	CrDatFrI.c					\
	CrDatFrP.c					\
	CrIFrBuf.c					\
	CrIFrDat.c					\
	CrIFrP.c					\
	CrPFrBuf.c					\
	CrPFrDat.c					\
	CrPFrI.c					\
	Image.c						\
	Info.c						\
	RdFToBuf.c					\
	RdFToDat.c					\
	RdFToI.c					\
	RdFToP.c					\
	WrFFrBuf.c					\
	WrFFrDat.c					\
	WrFFrI.c					\
	WrFFrP.c					\
	XpmI.h						\
	create.c					\
	data.c						\
	hashtab.c					\
	misc.c						\
	parse.c						\
	rgb.c						\
	rgbtab.h					\
	scan.c

EXTRA_DIST = amigax.c amigax.h simx.c simx.h
