#
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#

libmandir = $(LIB_MAN_DIR)

# Source files for X Input v2 man pages
XI2_txt = 					\
	XIAllowEvents.txt 			\
	XIBarrierReleasePointer.txt 		\
	XIChangeHierarchy.txt			\
	XIDefineCursor.txt			\
	XIGrabButton.txt			\
	XIGrabDevice.txt			\
	XIGrabEnter.txt				\
	XIQueryDevice.txt			\
	XIQueryPointer.txt			\
	XIQueryVersion.txt			\
	XISelectEvents.txt			\
	XISetClientPointer.txt			\
	XISetFocus.txt				\
	XIWarpPointer.txt			\
	XIListProperties.txt			\
	XIChangeProperty.txt

# Source files for X Input v1 man pages
libman_txt = \
	XAllowDeviceEvents.txt			\
	XChangeDeviceDontPropagateList.txt	\
	XChangeKeyboardDevice.txt		\
	XChangePointerDevice.txt		\
	XDeviceBell.txt				\
	XGetDeviceControl.txt			\
	XGetDeviceKeyMapping.txt		\
	XGetDeviceModifierMapping.txt		\
	XGetDeviceMotionEvents.txt		\
	XGetDeviceProperty.txt			\
	XGetExtensionVersion.txt		\
	XGetFeedbackControl.txt			\
	XGrabDeviceButton.txt			\
	XGrabDeviceKey.txt			\
	XGrabDevice.txt				\
	XListDeviceProperties.txt		\
	XListInputDevices.txt			\
	XOpenDevice.txt				\
	XQueryDeviceState.txt			\
	XSelectExtensionEvent.txt		\
	XSendExtensionEvent.txt			\
	XSetDeviceButtonMapping.txt		\
	XSetDeviceFocus.txt			\
	XSetDeviceMode.txt			\
	XSetDeviceValuators.txt			\
	$(XI2_txt)

# Name of DocBook XML files generated from .txt files by asciidoc
libman_xml = $(libman_txt:.txt=.xml)

# Shadow man pages are simply links to a main man page.
# They are created by the xmlto command when generating man pages from DocBook
# The shadow man page contains a gtroff .so request to include the main man page
XI2_shadows = 					\
	XIBarrierReleasePointers.man 		\
	XIUndefineCursor.man			\
	XIUngrabButton.man			\
	XIGrabKeycode.man			\
	XIUngrabKeycode.man			\
	XIUngrabDevice.man			\
	XIUngrabEnter.man			\
	XIGrabFocusIn.man			\
	XIUngrabFocusIn.man			\
	XIGetClientPointer.man			\
	XIGetFocus.man				\
	XIGetSelectedEvents.man			\
	XIDeleteProperty.man			\
	XIGetProperty.man			\
	XIFreeDeviceInfo.man			\
	XIGrabTouchBegin.man			\
	XIUngrabTouchBegin.man

libman_shadows =				\
	XGetDeviceDontPropagateList.man		\
	XChangeDeviceControl.man		\
	XChangeDeviceKeyMapping.man		\
	XSetDeviceModifierMapping.man		\
	XChangeFeedbackControl.man		\
	XUngrabDeviceButton.man			\
	XUngrabDeviceKey.man			\
	XUngrabDevice.man			\
	XDeviceTimeCoord.man			\
	XFreeDeviceList.man			\
	XCloseDevice.man			\
	XGetSelectedExtensionEvents.man		\
	XGetDeviceButtonMapping.man		\
	XGetDeviceFocus.man			\
	XChangeDeviceProperty.man		\
	XDeleteDeviceProperty.man		\
	$(XI2_shadows)

# Name of man page files generated from DocBook XML files by xmlto
libman_PRE = $(libman_txt:.txt=.man) $(libman_shadows)

if INSTALL_MANPAGES
libman_DATA = $(libman_PRE:.man=.$(LIB_MAN_SUFFIX))
endif

EXTRA_DIST = $(libman_txt) $(libman_PRE)
CLEANFILES = $(libman_DATA) *.libmansuffix
MAINTAINERCLEANFILES = $(libman_PRE) *.xml

# Dependencies for X Input v1 shadow pages
XGetDeviceDontPropagateList.man: XChangeDeviceDontPropagateList.man
XChangeDeviceControl.man: XGetDeviceControl.man
XChangeDeviceKeyMapping.man: XGetDeviceKeyMapping.man
XSetDeviceModifierMapping.man: XGetDeviceModifierMapping.man
XDeviceTimeCoord.man: XGetDeviceMotionEvents.man
XChangeFeedbackControl.man: XGetFeedbackControl.man
XUngrabDeviceButton.man: XGrabDeviceButton.man
XUngrabDeviceKey.man: XGrabDeviceKey.man
XUngrabDevice.man: XGrabDevice.man
XFreeDeviceList.man: XListInputDevices.man
XCloseDevice.man: XOpenDevice.man
XGetSelectedExtensionEvents.man: XSelectExtensionEvent.man
XGetDeviceButtonMapping.man: XSetDeviceButtonMapping.man
XGetDeviceFocus.man: XSetDeviceFocus.man
XChangeDeviceProperty.man: XGetDeviceProperty.man
XDeleteDeviceProperty.man: XGetDeviceProperty.man

# Dependencies for X Input v2 shadow pages
XIUndefineCursor.man: XIDefineCursor.man
XIUngrabButton.man XIGrabKeycode.man XIUngrabKeycode.man XIGrabTouchBegin.man XIUngrabTouchBegin.man: XIGrabButton.man
XIGetClientPointer.man: XISetClientPointer.man
XIGetFocus.man: XISetFocus.man
XIUngrabDevice.man: XIGrabDevice.man
XIGetProperty.man: XIChangeProperty.man
XIDeleteProperty.man: XIChangeProperty.man
XIUngrabEnter.man XIGrabFocusIn.man XIUngrabFocusIn.man: XIGrabEnter.man
XIGetSelectedEvents.man: XISelectEvents.man
XIFreeDeviceInfo.man: XIQueryDevice.man
XIBarrierReleasePointers.man: XIBarrierReleasePointer.man

# String replacements in MAN_SUBSTS now come from xorg-macros.m4 via configure
# Unable to use __libmansuffix__ as underscores are lost in txt --> xml conversion
MAN_SUBSTS += -e 's/libmansuffix/$(LIB_MAN_SUFFIX)/g'

# asciidoc is generating [FIXME...] values in the .TH line we should fix.
MAN_SUBSTS += -e 's/\[FIXME: source\]/$(XORG_MAN_PAGE)/'
MAN_SUBSTS += -e 's/\[FIXME: manual\]/XINPUT FUNCTIONS/'

# asciidoc generates shadow page references without the man section directory
MAN_SUBSTS += -e 's/^\.so X/.so man$(LIB_MAN_SUFFIX)\/X/'

SUFFIXES = .man .$(LIB_MAN_SUFFIX)
.man.$(LIB_MAN_SUFFIX):
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

# Generate man pages and shadow pages from .txt and then from DocBook XML
if HAVE_DOCTOOLS
SUFFIXES += .txt .xml
.txt.xml:
	$(AM_V_GEN)$(ASCIIDOC) -b docbook -d manpage -o $@ $<
.xml.man:
	$(AM_V_GEN)$(XMLTO) man $<
	$(AM_V_at)mv -f $(@:.man=.libmansuffix) $@

# Invoke asciidoc/xmlto main man page generation for shadow pages
$(libman_shadows):
	@if test ! -f $(@:.man=.libmansuffix); then \
	  rm -f $<; \
	  $(MAKE) $(AM_MAKEFLAGS) $< || exit 1; \
	fi
	$(AM_V_at)mv -f $(@:.man=.libmansuffix) $@
endif
