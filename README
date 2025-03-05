# jlcut
An extended replacement for GNU cut and FreeBSD cut.

While programming different amateur radios with a set of frequencies, tones
and options, I found that "cut" was inadequate.  So this is my own version
with extended functionality.
Jim Lemke (VE3LMK)


jlcut is generally a replacement for GNU cut and FreeBSD cut.
It is a clean implementation without reference to other source code.
I used the prefix jl to allow for a parallel installation with cut.

The major difference is writing out fields as specified on the command line.
  Fields are written in the argument order:
	echo 'A/B/C' | /usr/bin/cut -d/ -f 1,3,2
	A/B/C
	echo 'A/B/C' | jlcut -d/ -f 1,3,2
	A/C/B
  Fields can be written more than once:
	echo A,B,C | /usr/bin/cut -d, -f1,3,3
	A,C
	echo A,B,C | jlcut -d, -f1,3,3
	A,C,C

Other differences:
  -Option -w is supported, as on FreeBSD.
	echo A B C | jlcut -w -f-3
	A	B	C
  -Option -D allows changing the output field delimeter.
	echo A,B,C | jlcut -d, -D $'\t' -f-3
	A	B	C
  -Descending ranges are supported.
	echo A,B,C | jlcut -d, -f3-1
	C,B,A
  -A reference to field number 0 writes an empty field (a delimeter).
	echo 1,2,3 | jlcut -d, -f1,0,2-3
	A,,B,C

Build and install via:
  make
  [sudo] make install
The default install dir is $HOME/local if it exists, otherwise /usr/local.
For a different install dir use: make prefix=/install/dir install
If desired, "make install-cut" performs an install and creates a "cut" symlink
to "jlcut".
