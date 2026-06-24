lib_LTLIBRARIES = libXi.la

XI2_sources = XIAllowEvents.c \
		  XIGrabDevice.c \
		  XIQueryVersion.c \
		  XIQueryDevice.c \
		  XISetDevFocus.c \
		  XIGetDevFocus.c \
		  XIPassiveGrab.c \
		  XIProperties.c \
		  XISelEv.c \
		  XISetCPtr.c \
		  XIWarpPointer.c \
		  XIHierarchy.c \
		  XIDefineCursor.c \
		  XIQueryPointer.c \
		  XIBarrier.c

libXi_la_SOURCES = \
		  XAllowDv.c \
		  XChDProp.c \
		  XChgDCtl.c \
		  XChgFCtl.c \
		  XChgKbd.c \
		  XChgKMap.c \
		  XChgPnt.c \
		  XChgProp.c \
		  XCloseDev.c \
		  XDelDProp.c \
		  XDevBell.c \
		  XExtToWire.c \
		  XGetBMap.c \
		  XGetCPtr.c \
		  XGetDCtl.c \
		  XGetDProp.c \
		  XGetFCtl.c \
		  XGetKMap.c \
		  XGetMMap.c \
		  XGetProp.c \
		  XGetVers.c \
		  XGMotion.c \
		  XGrabDev.c \
		  XGrDvBut.c \
		  XGrDvKey.c \
		  XGtFocus.c \
		  XGtSelect.c \
		  XListDev.c \
		  XListDProp.c \
		  XOpenDev.c \
		  XQueryDv.c \
		  XSelect.c \
		  XSetBMap.c \
		  XSetDVal.c \
		  XSetMMap.c \
		  XSetMode.c \
		  XSndExEv.c \
		  XStFocus.c \
		  XUngrDev.c \
		  XUngrDvB.c \
		  XUngrDvK.c \
		  XExtInt.c  \
		  XIint.h \
		  $(XI2_sources)


libXi_la_LIBADD = $(XI_LIBS)

AM_CPPFLAGS = -I$(top_srcdir)/include \
              -I$(top_srcdir)/include/X11 \
              -I$(top_srcdir)/include/X11/extensions

AM_CFLAGS = $(XI_CFLAGS) \
            $(XFIXES_CFLAGS) \
            $(MALLOC_ZERO_CFLAGS) \
            $(CWARNFLAGS)


libXi_la_LDFLAGS = -version-number 6:1:0 -no-undefined

libXiincludedir = $(includedir)/X11/extensions
libXiinclude_HEADERS = $(top_srcdir)/include/X11/extensions/XInput.h \
		       $(top_srcdir)/include/X11/extensions/XInput2.h

EXTRA_DIST = XFreeLst.c
