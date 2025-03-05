/*
* Copyright 2025 James W. Lemke, MIT License
*
* jlcut is a replacement for GNU cut and FreeBSD cut.
* It is a scratch implementation.
* The major difference is that fields / bytes / chars are output in the
* requested order.  E.G.
* echo 1,2,3 | /usr/bin/cut -d, -f1,3,2
* 1,2,3
* echo 1,2,3 | jlcut -d, -f1,3,2
* 1,3,2
* echo 1,2,3 | /usr/bin/cut -d, -f1,3,3
* 1,3
* echo 1,2,3 | jlcut -d, -f1,3,3
* 1,3,3
*
* Change Log
* 2024-04-30 jwlemke Development started.
* 2024-05-10 jwlemke
*	Every selector is now stored as a range.
*	Decreasing ranges are now supporetd.
* 2024-06-06 jwlemke
*	You can now use selector 0 to insert an empty field.
* 2024-06-09 jwlemke
*	Add support for -D (debug).
*	Add support for -b (byte).
*	Re-order routines with main() earlier.
* 2024-06-11 jwlemke	Add support for -v (version).
* 2024-06-14 jwlemke	Change -v to -V.
* 2024-07-16 jwlemke	Add -D for output delimiter.
* 2024-07-17 jwlemke
*	For compatibility with FreeBSD cut, the -w default odelim is changed
*	from ' ' to '\t'.
* 2024-07-18 jwlemke	Move debug output to -x.
* 2024-08-12 jwlemke	Initial support for -c.
* 2024-09-10 jwlemke
*	Add support for -s.
*	Fixed empty field at end of line.
* 2024-09-12 jwlemke	Fix -c.
* 2024-10-27 jwlemke	Change format of version output.
* 2025-01-06 jwlemke	Fully implement descending ranges.
* 2025-01-07 jwlemke
*	Change error exit value to 1 as is conventional.
*	Add a man page.
* 2025-01-10 jwlemke 	Improve the Copyright notice.
* 2025-01-12 jwlemke
*	Check for MAXFIELDS overflow.
*	SelectorInit() was not called.
*	Stop scanning for delimeters once we don't need subsequent fields.
*	Fix for reverse ranges with byte & char specs beyond EOL.
* 2025-01-19 jwlemke	Set an MIT license.
*/

#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define VERMAJOR 2025
#define VERMINOR 305	// no leading zeros here!
#define VERPATCH 1

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#define MAXLINESZ 1024
#define MAXFIELDS 1024

void
version (void)
{
    fprintf (stdout,
      "jlcut V%04d.%04d.%d\n"
      "Copyright (C) 2025 James W. Lemke\n"
      "This is free software.  See LICENSE for copying conditions.\n"
      "There is NO warranty, "
      "not even for MERCHANTIBILTY or FITNESS FOR A PARTICULAR PURPOSE.\n"
      , VERMAJOR, VERMINOR, VERPATCH
      );
}

void
usage (void)
{
    version();
    fprintf (stdout,
      "\n"
      "Cut out selected portions of each line of a file.\n"
      "If no files are specified, stdin is read.\n"
      "Output is to stdout.\n"
      "\n"
      "cut -h\n"
      "cut -V\n"
      "cut -b <list> [-n] [<file>...]\n"
      "cut -c <list> [-n] [<file>...]\n"
      "cut -f <list> [-w | -d <delim>] [-D <delim>] [-n] [-s] [<file>...]\n"
      "\n"
      "-b, -c, -f specify byte, character or field mode\n"
      "-d specify a specific char as delimiter for -f (default tab)\n"
      "-D specify a char as output delimiter (default input delimiter)\n"
      "-h print a brief help message and quit\n"
      "-n is ignored\n"
      "-s supress lines with no field delimiter characters\n"
      "-V print version info and quit\n"
      "-w specify whitespace (space, tab) as delimiters\n"
      "   Multiple whitespace characters are treated as one.\n"
      "   Leading whitespace is ignored.\n"
      "-x print debug info for developers\n"
      );
      exit (1);
}


//
// prototypes & global data
//
int
ListParseOpt (char const * arg);

enum Mode {NONE, BYTE, CHAR, FIELD};

int optx = FALSE;

char *
NextChar (char *p);

int
NumParse (char const * p[]);

void
ProcessFile (FILE *fin, char *pgmname,
	     enum Mode mode, int opts, int optw, int optx,
	     const char *delim, const char odelim);

// Selectors (of bytes/chars/fields) wanted [0..selcnt-1]
// Parsed from the cmd line.
#define SELMAX 512
#define SELENDLINE (SELMAX - 1)
struct Selector {
    int begin;
    int end;
};
struct Selector selectors[SELMAX];
int selcnt = 0, selmax;

void
SelectorAppend (int begin, int end);
void
SelectorGet (int index, int *begin, int *end);
int
SelectorGetMax (void);
void
SelectorInit (void);
inline int
SelectorQty (void)
{
    return selcnt;
}

//
// The action starts here!
//
int
main (int argc, char *argv[])
{
    int opt;
    enum Mode mode = NONE;
    int modes = 0;
    char *delim = "\t";
    char odelim = ' ';
    int optd = FALSE;
    int opts = FALSE;
    int optw = FALSE;
    int i;
    FILE *fin;

    // Process cmd line options.
    while (TRUE) {
	opt = getopt (argc, argv, "b:c:d:D:f:hnsVwx");
	if (opt == -1) break;

	switch (opt) {
	case 'b':
	    mode = BYTE;
	    ++modes;
	    ListParseOpt (optarg);
	    break;
	case 'c':
	    mode = CHAR;
	    ++modes;
	    ListParseOpt (optarg);
	    break;
	case 'd':
	    optd = TRUE;
	    if (optw) usage();
	    delim = optarg;
	    delim[1] = '\0';
	    odelim = delim[0];
	    break;
	case 'D':
	    odelim = optarg[0];
	    break;
	case 'f':
	    mode = FIELD;
	    ++modes;
	    ListParseOpt (optarg);
	    break;
	case 'n':
	    // ignored
	    break;
	case 's':
	    opts = TRUE;
	    break;
	case 'V':
	    version ();
	    exit (0);
	    break;
	case 'w':
	    if (optd) usage();
	    optw = TRUE;
	    delim = "\t ";
	    odelim = '\t';
	    break;
	case 'x':
	    // Print debugging info.
	    optx = TRUE;
	    break;
	case 'h':
	case '?':
	default:
	    usage();
	}
    }
    if (modes != 1) {
	printf ("Error: Specify exactly one mode (-b -c -f).\n");
	usage();
    }

    if (optx) {
	// debug info
	printf ("selectors (#%d: 1 - %d):\n", SelectorQty(), SelectorGetMax());
	for (i=0; i<SelectorQty(); ++i) {
	    printf ("  #%d (%d %d)\n",
	    i, selectors[i].begin, selectors[i].end);
	}
	printf ("files (%d):\n", argc - optind);
	for (i=optind; i<argc; ++i) {
	    printf ("  %s\n", argv[i]);
	}
    }

    // If there are no files, use stdin.
    // Otherwise open each file.
    if (optind == argc) {
	fin = stdin;
	ProcessFile (fin, argv[0], mode, opts, optw, optx, delim, odelim);
    }
    else for (i=optind; i<argc; ++i) {
	fin = fopen (argv[i], "r");
	if (fin == NULL) {
	    fprintf (stderr, "%s: Error: %s: ", argv[0], argv[i]);
	    perror (NULL);
	    exit (1);
	}
	ProcessFile (fin, argv[0], mode, opts, optw, optx, delim, odelim);
	fclose (fin);
    }

    exit (0);
}

int
ListParseOpt (char const * arg)
{
    int num = 1;
    int rangeBegin, rangeEnd;
    char const *p;

    SelectorInit ();
    for (p=arg; *p; ) {
	rangeBegin = rangeEnd = 1;
	if (isdigit (*p)) {
	    // Number or beginning of range
	    num = NumParse (&p);
	    rangeBegin = rangeEnd = num;
	}
	if (*p == '-') {
	    // Range end is specified
	    ++p;
	    rangeEnd = SELENDLINE;
	    if (isdigit(p[0]))
		rangeEnd = NumParse (&p);
	    if (*p == '-') {
		fprintf (stderr,
		    "Error: Malformed range (%d-%d-).\n", rangeBegin, rangeEnd);
		exit (1);
	    }

	}
	SelectorAppend (rangeBegin, rangeEnd);
	if (*p == ',')
	    ++p;
	else
	    break;
    }

    return 0;
}

int
NumParse (char const * p[])
{
    char const * q = *p;
    int num = 0;

    while (isdigit(*q)) {
	num *= 10;
	num += *q - '0';
	++q;
    }
    if (num > MAXLINESZ) {
	fprintf (stderr,
	  "Error: byte/char/field num (%d) is too large\n", num);
	exit (1);
    }
    *p = q;
    return num;
}

void
SelectorInit (void)
{
    selmax = 0;
    selcnt = 0;
    selectors[0].begin = 0;
    selectors[0].end = 0;
}

void
SelectorAppend (int begin, int end)
{
    int i;

    if (selcnt >= SELMAX) {
	fprintf (stderr,
	  "Error: Too many byte/char/field specifications (%d).\n",
	  selcnt+1);
	exit (1);
    }

    if (begin <= end) {
	if (end   > selmax) selmax = end;
	selectors[selcnt].begin = begin;
	selectors[selcnt].end = end;
	++selcnt;
    }
    else {
	for (i=begin; i>=end; --i) {
	    SelectorAppend (i, i);
	}
    }
}

void
SelectorGet (int index, int *begin, int *end)
{
    *begin = selectors[index].begin;
    *end = selectors[index].end;
}

int
SelectorGetMax (void)
{
    return selmax;
}

void
ProcessFile (FILE *fin, char *pgmname,
	     enum Mode mode, int opts, int optw, int optx,
	     const char *delim, const char odelim)
{
    char *line = NULL;
    size_t linesz = 0;
    ssize_t linelen;
    size_t fieldsz;
    char *ifieldp[MAXFIELDS];
    int ifieldn[MAXFIELDS];
    int field, FieldQty = 0;
    char *pos, *pos2;
    size_t bytes;
    int outField;
    int i, range, done;

    while (! feof (fin)) {

	// Get another line.
	linelen = getline (&line, &linesz, fin);
	if (linelen < 0) {
	    if (feof (fin)) break;
	    fprintf (stderr, "%s: Error: ", pgmname);
	    perror (NULL);
	    exit (1);
	}
	if (linelen > 0 && line[linelen-1] == '\n') {
	    --linelen;
	    line[linelen] = '\0';
	}

	//
	// SCAN INPUT
	// For each field, store pointer & byte length in ifieldp[], ifieldn[].
	//
	switch (mode) {
	case BYTE:
	    ifieldp[0] = line;
	    ifieldn[0] = 0;
	    field = 0;
	    for (i=0; i<SelectorQty(); ++i) {
		int rangeBegin, rangeEnd;
		int len;

		SelectorGet (i, &rangeBegin, &rangeEnd);
		if (rangeBegin > linelen)
		    rangeBegin = linelen + 1;
		if (rangeEnd > linelen) {
		    rangeEnd = linelen + 1;
		    if (rangeBegin == rangeEnd) {
			// The whole field is beyond EOL so skip it.
			continue;
		    }
		}

		if (++field >= MAXFIELDS) {
		    fprintf (stderr,
		      "Error: Too many byte/char/fields in line.\n");
		    exit (1);
		}

		pos = line + rangeBegin - 1;
		len = rangeEnd - rangeBegin + 1;
		fieldsz = strlen (pos);
		if (len > fieldsz) len = fieldsz;
		ifieldp[field] = pos;
		ifieldn[field] = len;
	    }
	    FieldQty = field;
	    break;
	case CHAR:
	    ifieldp[0] = line;
	    ifieldn[0] = 0;
	    field = 0;
	    pos = line;
	    for (i=0; i<SelectorQty(); ++i) {
		int rangeBegin, rangeEnd;

		SelectorGet (i, &rangeBegin, &rangeEnd);
		if (rangeBegin > linelen)
		    rangeBegin = linelen + 1;
		if (rangeEnd > linelen) {
		    rangeEnd = linelen + 1;
		    if (rangeBegin == rangeEnd) {
			// The whole field is beyond EOL so skip it.
			continue;
		    }
		}

		if (++field >= MAXFIELDS) {
		    fprintf (stderr,
		      "Error: Too many byte/char/fields in line.\n");
		    exit (1);
		}
		pos2 = pos;
		range = rangeEnd - rangeBegin + 1;
		while (range && *pos2 != '\0')
		{
		    pos2 = NextChar(pos2);
		    --range;
		}

		ifieldp[field] = pos;
		ifieldn[field] = pos2 - pos;
		pos = pos2;
	    }
	    FieldQty = field;
	    break;
	case FIELD:
	    pos = line;
	    ifieldp[0] = pos;
	    ifieldn[0] = 0;
	    done = FALSE;
	    for (field=0; field<MAXFIELDS; ) {
		if (++field >= MAXFIELDS) {
		    fprintf (stderr,
		      "Error: Too many byte/char/fields in line.\n");
		    exit (1);
		}
		ifieldp[field] = NULL;
		ifieldn[field] = 0;
		if (optx)	// debug info
		    printf ("  scan field %d ", field);

		// For -w, skip leading white space.
		if (optw)
		    pos += strspn (pos, delim);

		// Scan for a delimiter or EOL.
		fieldsz = strcspn (pos, delim);
		if (optx)	// debug info
		    printf (" size %zu delim 0x%hhx\n", fieldsz, pos[fieldsz]);
		if (pos[fieldsz] == '\0')
		    done = TRUE;
		ifieldp[field] = pos;
		ifieldn[field] = fieldsz;
		pos += fieldsz + 1;
		if (done) break;
		// If we don't need subsequent fields,
		// don't keep scanning this line.
		if (field >= SelectorGetMax()) break;
	    }
	    FieldQty = field;

	    // Optionally ignore lines with no delimiters.
	    if (opts && FieldQty == 1) continue;
	    break;
	default:
	    fprintf (stderr, "%s: Error: internal error (mode=%d)\n",
		     pgmname, mode);
	    exit (1);
	}

	//
	// OUTPUT
	//
	if (optx)	// debug info
	    printf ("  FieldQty %d\n", FieldQty);
	if (mode != FIELD) {
	    // Write each range as one field, no delimeters.
	    for (i=1; i<=FieldQty; ++i) {
		if (ifieldn[i] == 0) continue;
		bytes = fwrite (ifieldp[i], 1, ifieldn[i], stdout);
		if (bytes < ifieldn[i]) {
		    fprintf (stderr, "%s: Error: cannot write to stdout.\n",
			    pgmname);
		    exit (1);
		}
	    }
	}
	else {
	    // Write delimited fields.
	    outField = 0;
	    for (i=0; i<SelectorQty(); ++i) {
		int rangeBegin, rangeEnd;
		int j;

		// Get a range of fields.
		SelectorGet (i, &rangeBegin, &rangeEnd);
		if (optx)	// debug info
		    printf ("  print range %d-%d: ", rangeBegin, rangeEnd);

		// Print each field.
		for (j=rangeBegin; j<=rangeEnd; ++j) {
		    if (j > FieldQty) break;

		    // Write a delimiter as a field separator.
		    if (++outField > 1) {
			bytes = putc (odelim, stdout);
			if (bytes < 1) {
			    fprintf (stderr, "%s: Error: cannot write to stdout.\n",
				    pgmname);
			    exit (1);
			}
		    }

		    // Write a field.
		    if (optx)	// debug info
			printf (" (Field %d)", j);
		    if (ifieldn[j] == 0) continue;
		    bytes = fwrite (ifieldp[j], 1, ifieldn[j], stdout);
		    if (bytes < ifieldn[j]) {
			fprintf (stderr, "%s: Error: cannot write to stdout.\n",
				pgmname);
			exit (1);
		    }
		}
	    }
	}

	// Terminate the line.
	bytes = putc ('\n', stdout);
	if (bytes < 1) {
	    fprintf (stderr, "%s: Error: cannot write to stdout.\n",
			pgmname);
	    exit (1);
	}
    }
}

// Advance past the current char, single byte or multi-byte.
// Unless the current char is null, the pointer always advances.
char *
NextChar (char *p)
{
    unsigned char *q = (unsigned char *)p;
    int len = 0;

    if (*q < 0x80)
	len = 1;		//ASCII (1-byte) char
    else if (*q < 0x88)
	len = 2;		//lead byte for 2-byte char
    else if (*q < 0x90)
	len = 3;		//lead byte for 3-byte char
    else if (*q < 0x98)
	len = 4;		//lead byte for 4-byte char
    else
	len = 1;		//shouldn't happen - fail safe

    // Don't advance past a null terminator.
    while (len) {
	if (*q == 0) break;
	++q;
	--len;
    }

    return (char *)q;
}
