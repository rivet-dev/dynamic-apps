#
# Copyright (c) 2007, Oracle and/or its affiliates.
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

libman_xml = XTest.xml

libman_PRE =	\
	XTestQueryExtension.man \
	XTestCompareCursorWithWindow.man \
	XTestCompareCurrentCursorWithWindow.man \
	XTestFakeKeyEvent.man \
	XTestFakeButtonEvent.man \
	XTestFakeMotionEvent.man \
	XTestFakeRelativeMotionEvent.man \
	XTestGrabControl.man \
	XTestSetGContextOfGC.man \
	XTestSetVisualIDOfVisual.man \
	XTestDiscard.man

EXTRA_DIST = $(libman_PRE) $(libman_xml) x.stamp

CLEANFILES = $(libman_DATA)

MAINTAINERCLEANFILES = $(libman_PRE)

if INSTALL_MANPAGES
libman_DATA = $(libman_PRE:man=@LIB_MAN_SUFFIX@)
endif

# String replacements in MAN_SUBSTS now come from xorg-macros.m4 via configure
SUFFIXES = .$(LIB_MAN_SUFFIX) .man

.man.$(LIB_MAN_SUFFIX):
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

# This is ugly, but handling of tools with multiple outputs in make is a
# huge PITA. The commands below are taken from the automake manual:
# http://www.gnu.org/software/automake/manual/automake.html#Multiple-Outputs
if HAVE_XMLTO
x.stamp: $(libman_xml)
	@rm -f x.tmp
	@touch x.tmp
	$(XMLTO) man $^
	@for man in $(libman_PRE); do \
	  out=`echo $$man | sed 's/\.man/.__libmansuffix__/'`; \
	  echo mv -f $$out $$man; \
	  mv -f $$out $$man || exit 1; \
	done
	@mv -f x.tmp $@
$(libman_PRE): x.stamp
	@if test -f $@; then :; else \
	  rm -f $<; \
	  $(MAKE) $(AM_MAKEFLAGS) $<; \
	fi
CLEANFILES += x.tmp
MAINTAINERCLEANFILES += x.stamp
endif # HAVE_XMLTO
