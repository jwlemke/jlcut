# Copyright 2025 James W. Lemke, MIT License
SHELL = /bin/sh
.PHONY : all clean install install-cut

# Common prefix for installation directories.
# NOTE: This directory must exist when you start the install.
prefix = /usr/local
ifneq ($(wildcard ~/local/bin),)
	prefix = ~/local
endif
datarootdir = $(prefix)/share
datadir = $(datarootdir)
exec_prefix = $(prefix)
# Where to put the executable
bindir = $(exec_prefix)/bin
# Where to put the Info files.
infodir = $(datarootdir)/info

all: jlcut

jlcut: jlcut.c
	gcc -O2 -Wall -o jlcut jlcut.c

install: jlcut
	mv -f jlcut $(bindir)/jlcut
	install -m 0644 jlcut.1 $(datadir)/man/man1/
	gzip -f $(datadir)/man/man1/jlcut.1 

install-cut: install
	test -f $(bindir)/cut -o -L $(bindir)/cut ||\
	  ln -s $(bindir)/jlcut $(bindir)/cut

ranges: ranges.c
	gcc -O1 -o ranges ranges.c

clean:
	rm -f jlcut.o jlcut ranges.o ranges
