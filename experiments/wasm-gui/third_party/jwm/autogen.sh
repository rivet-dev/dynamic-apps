#!/bin/sh
aclocal
automake -ac
autoreconf -fi
touch config.rpath
