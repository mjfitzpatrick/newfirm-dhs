
static char rcsid[] =
	"$Id: winxpvm.c,v 4.50 1998/04/09 21:12:15 kohl Exp $";

/* 
 * winxpvm.c (modified from tk/win/winMain.c) --
 *
 *	Main entry point for XPVM Windows.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) winMain.c 1.27 96/04/11 17:50:25
 */

#include <tk.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <malloc.h>
#include <locale.h>

/*
 * Forward declarations for procedures defined later in this file:
 */

static void XpvmPanic _ANSI_ARGS_( TCL_VARARGS( char *, format ) );

int Xpvm_Init( Tcl_Interp * );

/*
 *----------------------------------------------------------------------
 *
 * WinMain --
 *
 *	Main entry point from Windows.
 *
 * Results:
 *	Returns false if initialization fails, otherwise it never
 *	returns. 
 *
 * Side effects:
 *	Just about anything, since from here we call arbitrary Tcl code.
 *
 *----------------------------------------------------------------------
 */

int APIENTRY
WinMain( hInstance, hPrevInstance, lpszCmdLine, nCmdShow )
    HINSTANCE hInstance;
    HINSTANCE hPrevInstance;
    LPSTR lpszCmdLine;
    int nCmdShow;
{
    char buffer[MAX_PATH];
    char **argv;
	char **argvlist;
	char *p;
    int argc;
	int size;
	int i;

    /*
     * Set up the default locale to be Windows ANSI character set.
     */

    setlocale( LC_ALL, "" );

    Tcl_SetPanicProc( XpvmPanic );

    /*
     * Increase the application queue size from default value of 8.
     * At the default value, cross application SendMessage of
	 * WM_KILLFOCUS will fail because the handler will not be able
	 * to do a PostMessage!  This is only needed for Windows 3.x,
	 * since NT dynamically expands the queue.
     */
    SetMessageQueue( 64 );

    /*
     * Create the console channels and install them as the standard
     * channels.  All I/O will be discarded until TkConsoleInit is
     * called to attach the console to a text widget.
     */

    TkConsoleCreate();

    /*
     * Precompute an overly pessimistic guess at the number of arguments
     * in the command line by counting non-space spans.  Note that we
     * have to allow room for the executable name and the trailing NULL
     * argument (and extra dummy "-" arg for XPVM hostfile).
     */

    for ( size = 4, p = lpszCmdLine; *p != '\0'; p++ )
	{
		if ( isspace(*p) )
		{
	    	size++;
	    	while ( isspace(*p) ) p++;
	    	if ( *p == '\0' )
				break;
		}
    }

    argvlist = (char **) ckalloc((unsigned) (size * sizeof(char *)));
    argv = argvlist;

    /*
     * Parse the Windows command line string.  If an argument begins
	 * with a double quote, then spaces are considered part of the
	 * argument until the next double quote.  The argument terminates
	 * at the second quote.  Note that this is different from the usual
	 * Unix semantics.
     */

    for ( i=2, p = lpszCmdLine; *p != '\0'; i++ )
	{
		while ( isspace(*p) ) p++;

		if ( *p == '\0' ) break;

		if ( *p == '"' )
		{
	    	p++;
	    	argv[i] = p;
	    	while ( *p != '\0' && *p != '"' ) p++;
		}
		else
		{
	    	argv[i] = p;
	    	while ( *p != '\0' && !isspace(*p) ) p++;
		}

		if ( *p != '\0' )
		{
	    	*p = '\0';
	    	p++;
		}
    }

    argv[i] = NULL;
    argc = i;

    /*
     * Since Windows programs don't get passed the command name as the
     * first argument, we need to fetch it explicitly.
     */

    GetModuleFileName(NULL, buffer, sizeof(buffer));
    argv[0] = buffer;

	/* Stuff in Dummy Arg for Hostfile */

	argv[1] = trc_copy_str( "-" );

	/* Pass to TK */

    Tk_Main(argc, argv, Tcl_AppInit);

    return 1;
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 *	This procedure performs application-specific initialization.
 *	Most applications, especially those that incorporate additional
 *	packages, will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in interp->result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AppInit( interp )
    Tcl_Interp *interp;		/* Interpreter for application. */
{
    if ( Tcl_Init( interp ) == TCL_ERROR )
		goto error;

    if ( TclHasSockets( interp ) == TCL_ERROR ) 
		goto error;

    if ( Tk_Init( interp ) == TCL_ERROR )
		goto error;

	if ( Xpvm_Init( interp ) == TCL_ERROR )
		goto error;

    Tcl_StaticPackage( interp, "Tk", Tk_Init,
		(Tcl_PackageInitProc *) NULL );

	Tcl_StaticPackage( interp, "XPVM", Xpvm_Init,
		(Tcl_PackageInitProc *) NULL );

    Tcl_SetVar( interp, "tcl_rcFileName", "~/.xpvmrc",
		TCL_GLOBAL_ONLY );

    /*
     * Initialize the console only if we are running as an interactive
     * application.
     */

    if ( strcmp( Tcl_GetVar( interp, "tcl_interactive",
		TCL_GLOBAL_ONLY ), "1" ) == 0 )
	{
		if ( TkConsoleInit( interp ) == TCL_ERROR )
			goto error;
    }

    return TCL_OK;

error:

    WishPanic(interp->result);

    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * XpvmPanic --
 *
 *	Display a message and exit.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Exits the program.
 *
 *----------------------------------------------------------------------
 */

void
XpvmPanic TCL_VARARGS_DEF(char *,arg1)
{
    va_list argList;
    char buf[1024];
    char *format;
    
    format = TCL_VARARGS_START(char *,arg1,argList);
    vsprintf(buf, format, argList);

    MessageBeep(MB_ICONEXCLAMATION);
    MessageBox(NULL, buf, "Fatal Error in XPVM",
	    MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
    ExitProcess(1);
}
