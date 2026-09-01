#!/usr/bin/perl -w
#
# availability.pl -- list the Darwin OS versions, for xnu's header
# generators.
#
# xnu's bsd/sys/make_symbol_aliasing.sh turns a list of released OS
# versions into the __DARWIN_ALIAS_STARTING_* macros that sys/cdefs.h
# needs, and reads that list by running <sdk>/usr/local/libexec/
# availability.pl.  Apple ships that script only inside their internal
# SDK, so a tree built from the open-source releases alone has the
# generator and not its input.
#
# What the script wants is a list of version numbers, one per line, and
# nothing else -- a set of release numbers rather than anything of
# Apple's.  That is what this prints.  A version that never shipped
# costs an unused macro definition; one that is missing costs a header
# that will not compile, so the range is generous on purpose.
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

use strict;

my $what = $ARGV[0] || '';

sub emit_range {
    my ($major_lo, $major_hi, $minor_hi) = @_;
    for my $major ($major_lo .. $major_hi) {
        for my $minor (0 .. $minor_hi) {
            print "$major.$minor\n";
        }
    }
}

if ($what eq '--macosx') {
    # 10.0 through 10.15, then the single-number releases from 11 on.
    print "10.$_\n" for (0 .. 15);
    emit_range(11, 26, 7);
} elsif ($what eq '--ios') {
    emit_range(2, 26, 7);
} elsif ($what eq '--watchos') {
    emit_range(1, 26, 7);
} elsif ($what eq '--tvos' || $what eq '--appletvos') {
    emit_range(9, 26, 7);
} elsif ($what eq '--bridgeos') {
    emit_range(2, 26, 7);
} elsif ($what eq '--visionos' || $what eq '--xros') {
    emit_range(1, 26, 7);
} else {
    print STDERR "usage: $0 --macosx|--ios|--watchos|--tvos|--bridgeos|--visionos\n";
    exit 1;
}
