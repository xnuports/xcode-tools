#!/bin/sh
#
# xar-make.sh [make arguments]
#
# Run xar's make with its own header reachable the way its sources
# spell it.
#
# src/xar_internal.h says #include <xar/xar.h>, which is the installed
# layout: Apple ship usr/include/xar/xar.h.  Nothing in the build tree
# has that shape -- configure generates include/xar.h, one directory
# short -- and no rule in the Makefile bridges the two, so a build from
# a clean tree cannot compile src/ at all.
#
# Pointing a "xar" symlink at the directory that already holds xar.h
# gives <xar/xar.h> somewhere to land, without touching the sources or
# the generated Makefile.
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

set -e

if [ -d include ] && [ ! -e include/xar ]; then
	ln -sfn . include/xar
fi

exec make "$@"
