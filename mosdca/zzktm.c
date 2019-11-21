#include <stdio.h>
#include <stdarg.h>
#include <tcl.h>
#include "dso.h"
#include "kwdb.h"


/*
 * ZZKTM -- Dummy framework for testing Keyword Translation Modules.
 *
 * Usage:	
 *	    zzktm <ktmfile>
 *	    zzktm [<initfile>] <ktmfile>
 *	    zzktm [<initfile>] <ktmfile> [<donefile>]
 */

int debug = 2;
FILE *debug_out;

static char *dprintf(int level, char *name, char *fmt, ...);
static int tcl_print();
static int tcl_dprintf();


main (argc, argv)
int argc;
char **argv;
{
	char *initfile, *ktmfile, *donefile;
	Tcl_Interp *tcl;

	if (argc < 2) {
	    fprintf (stderr,
	    "Usage: zzktm [<initfile>] <ktmfile> [<donefile>]\n");
	    exit (1);
	}

	debug_out = stderr;

	if (!(tcl = Tcl_CreateInterp()))
	    exit (2);

	/* Load the KWTB Tcl module. */
	kwdb_TclInit (tcl);

	/* Load local commands. */
	Tcl_CreateCommand (tcl, "print", tcl_print, NULL, NULL);
	Tcl_CreateCommand (tcl, "dprintf", tcl_dprintf, NULL, NULL);

	if (argc >= 3) {
	    initfile = argv[1];
	    if (access (initfile, 0) == 0)
		run (tcl, initfile);
	    else
		fprintf (stderr, "Warning: cannot open %s\n", initfile);
	}

	if (argc >= 2) {
	    ktmfile = (argc == 2) ? argv[1] : argv[2];
	    if (access (ktmfile, 0) == 0)
		run (tcl, ktmfile);
	    else
		fprintf (stderr, "Warning: cannot open %s\n", ktmfile);
	}

	if (argc >= 4) {
	    donefile = argv[3];
	    if (access (donefile, 0) == 0)
		run (tcl, donefile);
	    else
		fprintf (stderr, "Warning: cannot open %s\n", donefile);
	}
}


/* RUN -- Execute a Tcl script.
 */
run (tcl, fname)
Tcl_Interp *tcl;
char *fname;
{
	int status;

	fprintf (stderr, "---------- executing %s -----------\n", fname);
	status = Tcl_EvalFile (tcl, fname);

	if (status == TCL_OK)
	    fprintf (stderr, "no errors\n");
	else {
	    fprintf (stderr, "Error on line %d: %s\n",
		tcl->errorLine, tcl->result);
	}
}


/* TCL_PRINT -- Simple Tcl callable print command.
 */
static int
tcl_print (client_data, tcl, argc, argv)
void *client_data;
Tcl_Interp *tcl;
int argc;
char **argv;
{
	register int i;

	for (i=1;  i < argc;  i++) {
	    fprintf (stderr, "%s", argv[i]);
	    if (argc > 2)
		fprintf (stderr, " ");
	}
	fflush (stderr);

	return (TCL_OK);
}


/* TCL_DPRINTF -- Tcl interface to the DCA dprintf command, used for debug
 * and error output.
 *
 * Usage:	dprintf level name format [args ...]
 *
 * Level is the integer debug level of the message.  Name is the message name
 * prefix.
 */
static int
tcl_dprintf (client_data, tcl, argc, argv)
void *client_data;
Tcl_Interp *tcl;
int argc;
char **argv;
{
	char *name, *format;
	int level, o;

	if (argc < 4)
	    return (TCL_ERROR);

	level = atoi (argv[1]);
	name = argv[2];
	format = argv[3];
	o = 4;

	switch (argc) {
	case 4:
	    dprintf (level, name, format);
	    break;
	case 5:
	    dprintf (level, name, format, argv[o]);
	    break;
	case 6:
	    dprintf (level, name, format, argv[o], argv[o+1]);
	    break;
	case 7:
	    dprintf (level, name, format, argv[o], argv[o+1], argv[o+2]);
	    break;
	case 8:
	    dprintf (level, name, format, argv[o], argv[o+1], argv[o+2],
		argv[o+3]);
	    break;
	case 9:
	    dprintf (level, name, format, argv[o], argv[o+1], argv[o+2],
		argv[o+3], argv[o+4]);
	    break;
	default:
	    return (TCL_ERROR);
	}

	return (TCL_OK);
}


/* DPRINTF -- Format and print a debug message.
 *
 *   Usage: dprintf (level, name, fmt, args)
 *
 * The output line will be of the form "name: text", where text is the
 * string generated by executing a printf using fmt and args.  If name=NULL
 * the only the formatted text is output, e.g. to continue printing on the
 * same line of output.  Output is generated only if the current debug level
 * equals or exceeds the level of the message.  The output is automatically
 * flushed for level=1 messages, higher level messages are buffered.
 */
static char *
dprintf (int level, char *name, char *fmt, ...)
{
	va_list args;


	va_start (args, fmt);

	if (debug >= level) {
	    if (name) {
		/* Print the debug level. */
		(void) fprintf (debug_out, "%d ", level);

		/* Print name of function which output is from. */
		(void) fprintf (debug_out, "%s: ", name);
	    }

	    /* Print out remainder of message. */
	    (void) vfprintf (debug_out, fmt, args);

	    if (level <= 1)
		fflush (debug_out);
	}
	va_end (args);
}