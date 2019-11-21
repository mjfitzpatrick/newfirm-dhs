#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __POSIX__
#include <sys/select.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#endif
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <stddef.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>


#include "super.h"


#define  IO_DEBUG    (getenv("IO_DEBUG")!=NULL||access("/tmp/IO_DEBUG",F_OK)==0)
#define  SELWIDTH    32

    
char    imagetype[SZ_FNAME];

extern	int console, verbose;

char 	*mytok();
int	mbusMsgHandler  (void *client_data, int fd, int id);
int	stdinMsgHandler (void *client_data, int fd, int id);
	    
extern char *supMakeKeywordList ();


XtPointer sup_addInput();
clientIoChanPtr supOpenInet();
    
supDataPtr sup_g;	/* global version */

int	dca_done	= -999;
time_t	s_time		= (time_t) 0;
time_t	e_time		= (time_t) 0;

int 	supTaskExited (int mid);
int 	supHostAdded (int mid);
int 	supHostDeleted (int mid);




/*****************************************************************************
**  SUPINITIO -- Open a public inet port to the used for incoming
**  client connections.
*/
void
supInitIO (sup)
supDataPtr sup;
{
    int    mb_fd;


    /*  Open the public inet port used by the PANs and other clients
     *  to talk directly to the Supervisor.
     */
    if (IO_DEBUG) 
	fprintf (stderr, "Opening public port %d...sup=0x%x\n", sup->port, sup);
    bcopy (supOpenInet(sup), &sup->pub_chan, sizeof (clientIoChan));


    /*  Add a handler for the message bus commands.
     */
    if ((mb_fd = mbGetMBusFD()) >= 0) {
        if (IO_DEBUG) 
	    fprintf (stderr, "Opening message bus fd=%d....sup=0x%x\n",
		mb_fd, sup);
	sup->mbus_chan = (clientIoChanPtr) clientGetChannel (sup);
	sup->mbus_chan->sup = sup;
	sup->mbus_chan->connected = 1;
	sup->mbus_chan->datain  = mb_fd;
	sup->mbus_chan->dataout = mb_fd;
	sup->mbus_chan->id = sup_addInput (sup, mb_fd, mbusMsgHandler,
	    (XtPointer) sup);
    } else
	perror ("Error adding MBUS input handler.");

    /*  Add a handler for standard input (console) commands.
    if (IO_DEBUG) 
	fprintf (stderr, "Opening stdin fd=%d....\n", fileno(stdin));
    if (sup->use_console) {
        sup->stdin_chan = (clientIoChanPtr) clientGetChannel (sup);
        sup->stdin_chan->sup = sup;
        sup->stdin_chan->connected = 1;
        sup->stdin_chan->dataout = fileno(stdin);
        sup->stdin_chan->datain  = fileno(stdin);
        sup->stdin_chan->id = sup_addInput (sup, fileno(stdin), stdinMsgHandler,
	    (XtPointer) sup);
    }
     */

    sup_g = sup;
}



/*****************************************************************************
**  SUP_ADDINPUT -- Register a procedure to be called when there is input
**  to be processed on the given input source.  The Supervisor code doesn't
**  talk to X directly, so we need to provide this interface routine to handle
**  messages coming from the clients and the message bus.
*/
XtPointer
sup_addInput (sup, input, proc, client_data)
supDataPtr sup;
int input;
void (*proc)();
XtPointer client_data;
{
    extern XtAppContext app_context;		/* see super.c		*/

    XtPointer id = (XtPointer) XtAppAddInput (app_context, input,
/*
	(XtPointer) (XtInputReadMask | XtInputWriteMask),
*/
	(XtPointer) (XtInputReadMask),
	*proc, client_data);

    if (IO_DEBUG)
	fprintf (stderr, "\taddInput: fd=%d  proc=0x%x  id=0x%x  client=0x%x\n",
    	    input, proc, id, client_data);
    return ((XtPointer) id);
}


/*****************************************************************************
**  SUP_REMOVEINPUT -- Remove a callback previously posted with sup_addInput.
*/
void
sup_removeInput (sup, id)
supDataPtr sup;
XtPointer id;
{
    if (IO_DEBUG)
	fprintf (stderr, "\tremoveInput:  id=0x%x\n", id);

    XtRemoveInput ((XtInputId)id);
}



/*****************************************************************************
*/
int mbusMsgHandler (void *data, int fd, int id)
{
    char *from = NULL, *to = NULL, *host = NULL, *msg = NULL;
    int  from_tid, to_tid, subject;
    int  mytid = mbAppGet(APP_TID);



    /*
    if (IO_DEBUG)
	fprintf (stderr,"mbusMsgHandler data = 0x%x 0x%x 0x%x\n", data, fd, id);
    */

    to_tid = subject = -1;
    if (mbusRecv(&from_tid, &to_tid, &subject, &host, &msg) < 0) {
        printf("Error in mbusRecv()\n");
        if (msg)  free (msg);
        if (host) free (host);
	return (ERR);
    }

    /* See if message was meant specifically for me.... */
    if (to_tid == mytid && subject != MB_ERR) {
        supIOHandler ((supDataPtr) data, from_tid, subject, msg);

    } else if (to_tid < 0) {
	if (IO_DEBUG)
            fprintf (stderr, "BCAST:  from:%d  subj:%d msg='%s'\n",
		from_tid, subject, msg);
        supIOHandler ((supDataPtr) data, from_tid, subject, msg);

    } else {
	if (IO_DEBUG) {
            fprintf (stderr, "Monitor...\n");
            fprintf (stderr, "   from:%d\n   to:%d\n   subj:%d\n",
		from_tid, to_tid, subject);
            fprintf (stderr, "   host:'%s'\n   msg='%s'\n", host, msg);
	}
    }

    if (msg)  free (msg);
    if (host) free (host);

    return (OK);
}


/*****************************************************************************
*/
int stdinMsgHandler (void *data, int fd, int id)
{
    int n;
    char buf[2048];
    int  mytid = mbAppGet(APP_TID);


    n = read(fd, buf, 2048);		/* already saved fileno(stdin)	*/

    buf[strlen(buf) - 1] = (char)NULL;	/* kill newline  		*/
    if (IO_DEBUG)
        fprintf (stderr, "stdin: n=%d  msg='%s'\n", n, buf);

    if (n < 0 || strncmp (buf, "quit", 4) == 0) {
	mbusDisconnect (mytid);
        exit (0);

    } else {
	/* Pass off to our main message handler.
	 */
        inputEventHandler (buf);
    }

    return (OK);
}



/*****************************************************************************
**  Local Message Handler
*/
supIOHandler(supDataPtr sup, int from, int subject, char *msg)
{
    int   from_tid, to_tid, subj;
    int   tid = 0, pid = 0, port = 0, clid;
    int   mytid = mbAppGet (APP_TID);
    char  who[128], host[128], reply[128], buf[128];
    char  *txt, *me = (char *)mbAppGetName();
    double expID;

    static int    npxf_started 	   = 0;
    static int    npxf_finished    = 0;
    static int    npxf_transferred = 0;
    static time_t now 		   = (time_t) 0;
    static time_t then 		   = (time_t) 0;
    static time_t t2, t1;


    if (0 && IO_DEBUG && subject != MB_ACK) {
	fprintf (stderr, "supIOHandler: from=%d  subj=%d  msg='%s'\n", 
	    from, subject, msg);
        logMessage (sup, from, subject, msg);
    }


    switch (subject) {
    case MB_CONNECT:

	mbParseConnectMsg (msg, &tid, &who, &host, &pid);
	if (IO_DEBUG)
	    fprintf (stderr, "CONNECT on %s: %s  tid=%d\n", me, msg, tid);


	/*  If it's the supervisor connecting, and we don't already have an
	 *  established connection to the Super, set it up now.
	 */
	if (isSupervisor (who) && mbAppGet (APP_STID) < 0) {
	    mytid = mbAppGet(APP_TID);
	    if (mbConnectToSuper (mytid, tid, host, pid) == OK)
		mbAppSet (APP_TID, abs(mytid));
	}

	/*  When we get a CONNECT message, post a notifier so we're
	 *  alerted whent the task exits.
	 */
	mbAddTaskExitHandler (tid, supTaskExited);

	/*  Add the client to the process table.
	*/
	procAddClient (sup, (clientIoChanPtr)NULL, who, host, port,
	    tid, pid, "Connected", TRUE);
        if (strncasecmp (who, "SMCMgr", 6) == 0) {
	    char buf[SZ_LINE];

            sup->numProcMgrs++;
	    bzero (buf, SZ_LINE);
            sprintf (buf, "option dirname %s\n\0", sup->rawDir);
            mbusBcast ("SMCMgr", buf, MB_SET);
	    sup_message (sup, "rdirname", sup->rawDir);
	}

	break;

    case MB_EXITING:

	mbParseConnectMsg (msg, &tid, &who, &host, &pid);
	if (IO_DEBUG)
	    fprintf (stderr, "DISCONNECT on %s from %s; tid=%d  pid=%d\n",
	 	me, who, tid, pid);

	procDisconnect (sup, procFindClient (sup, -1, tid));
	break;

    case MB_READY:

	sup->nready++;

	if (IO_DEBUG)
	    fprintf (stderr, "READY on %s: %d of %d (from: %d)\n", me, 
		sup->nready, sup->nclients, from);

	if (sup->nready == sup->nclients) {
	    char *keywords;


	    /* Update the Supervisor status.
	    */
            if (procUpdateStatus(sup, 0, "Clients Ready...", 1) < 0)
                fprintf (stderr, "Error updating status table.\n");

	    if (console) {
	        fprintf (stderr, "\n***************************\n");
	        fprintf (stderr, "\n  ALL CLIENTS REPORT READY \n");
	        fprintf (stderr, "\n***************************\n");
	    }

	    /* Clients are all connected, send the keyword list to the
	    ** PXF clients before continuing.
	    */
	    keywords = supMakeKeywordList ();
	    mbusBcast ("PXF", keywords, MB_SET);
	    free (keywords);
	}

    case MB_SET:
	if (IO_DEBUG) {
	    now = time((time_t)NULL);
	    if (strncmp(msg, "seg", 3)!=0 && strncmp(msg,"dca_trans",9) != 0)
	        fprintf (stderr,"%d: SET on %s: '%-70.70s' tid=%d\n", 
		    (now-then), me, msg, from);
	    then = now;
	}

	if (strncmp (msg, "segList", 7) == 0) {
	    sup_message (sup, "smcStat", &msg[7]);
	    break;

	} else if (strncmp (msg, "segCount", 8) == 0) {
	    sup_message (sup, "smcCount", &msg[9]);
	    break;

	} else if (strncmp (msg, "rtdStat", 7) == 0) {
	    sup_message (sup, "rtdStat", &msg[7]);
	    break;

	} else if (strncmp (msg, "dca_tid", 7) == 0) {
	    sup->dca_tid = atoi (&msg[8]);
	    mbusBcast ("PXF", msg, MB_SET);
	    break;

	} else if (strncmp (msg, "dca_done ", 9) == 0) {
	    expID = (double) atof (&msg[9]);

t1 = time((time_t)NULL);
	    if (console)
	        fprintf (stderr, "DCA Complete: ending readout: %.6lf\n",
	    	    expID);

	    /* Flag the exposure as done.
	    */
    	    pqSetStat (sup, expID, PQ_DONE);

	    /* Send the image type to the SMC Mgr.
	    */
/* For Sky Subtraction
	    sprintf (reply, "imtype %.6lf %s\0", expID, imagetype);
	    if (console)
	        fprintf (stderr, "SMCMgr ending: %s\n", reply);
	    mbusBcast ("SMCMgr", reply, MB_SET);
*/

	    /* Tell the SMCMgr to finish with this ExpID, i.e. delete
	    ** the pages.
	    */
	    sprintf (reply, "ExpID %.6lf\0", expID);
	    if (console)
	        fprintf (stderr, "SMCMgr ending: %s\n", reply);
	    mbusBcast ("SMCMgr", reply, MB_FINISH);

	    /* Update the status in the GUI.
	    sup_message (sup, "dcaStat", &msg[9]);
	    */
	    {   char *ip, *op, exp[SZ_LINE], fname[SZ_FNAME];
		int  i, len = strlen (msg);

		/* Extract filename from message */
		for (ip=&msg[len-1]; *ip != '/'; ip--)
		    ;
		bzero (fname, SZ_FNAME);
		strcpy (fname, ip);

		bzero (exp, SZ_LINE);
		for (ip=&msg[9], op=exp; ip && !isspace(*ip); )
		    *op++ = *ip++;

		len = strlen (sup->rawDir);
		if (sup->rawDir[len-1] == '/')
	            sprintf (reply, "%s %s%s\0", exp, sup->rawDir, fname);
		else
	            sprintf (reply, "%s %s/%s\0", exp, sup->rawDir, fname);
	        sup_message (sup, "dcaStat", reply);

	        sprintf (reply, "%s %s\0", &msg[9], sup->rawDir);
	        sup_message (sup, "dca_done", reply);
	    }

	    break;

	} else if (strncmp (msg, "dca_trans_done", 14) == 0) {
	    int seqno = atoi (&msg[15]);

	    npxf_transferred++;
	    if (console)
	        fprintf (stderr, "Transfer Complete: ending readout: %d\n",
	    	    npxf_transferred);
	    if (npxf_transferred >= sup->numProcMgrs) {
	        if (console)
	            fprintf (stderr, "Sending EndReadout....***************\n");
	        ndhsEndReadout (sup->dca_tid, seqno);
	        npxf_transferred -= sup->numProcMgrs;
	    }
	    break;

	} else if (strncmp (msg, "dca_trans", 9) == 0) {
	    sup_message (sup, "dcaTransStat", &msg[9]);
	    break;

	} else if (strncmp (msg, "dca_cwd", 7) == 0) {
	    strcpy (sup->rawDir, &msg[8]);
	    sup_message (sup, "dirname", &msg[8]);
	    break;

	} else if (strncmp (msg, "debug", 5) == 0) {
	    /* We'll let the SMC Manager handle the debug flags remotely.
	    */
	    mbusBcast ("SMCMgr", &msg[6], MB_SET);
	    break;

	} else if (strncmp (msg, "imagetype", 9) == 0) {
	    char *ip = &msg[10], *op;
	    expID = (double) atof (ip);

	    while (*ip && !isspace(*ip)) ip++;	/* skip expID 		      */
	    while (*ip &&  isspace(*ip)) ip++;	/* skip spaces 		      */

	    bzero (imagetype, SZ_FNAME);
	    for (op=imagetype; *ip && !isspace(*ip); )	/* copy lower case    */
		*op++ = tolower(*ip++);
	    *op = '\0';

            /*  Tell the DCA to get ready. This is the first message generated
	    **  when we begin processing and will always return a value.  If 
	    **  no type-specific keyword is found the default 'image' will be
	    **  used.
            sup->imgSeqNo++;
            */
t2 = time((time_t)NULL);
/*
fprintf (stderr, "Time [%d] between [expID = %.6lf]....\n",
    (t2-t1), expID);
*/
	
	    supSeqNo(sup->imgSeqNo);			/* update seqno file */
	    sup_msgi (sup, "seqno", sup->imgSeqNo);	/* update GUI	     */
	    /*
	    sup_msgi (sup, "seqno", supSeqNo(sup->imgSeqNo));
	    */
	    if (console)
	        fprintf (stderr, 
		    "****** imagetype: '%s' seqno: %d expID: %.6lf\n", 
		    imagetype, sup->imgSeqNo, expID);

/*
	    sprintf (buf, "seqno %d\0", sup->imgSeqNo);
	    mbusBcast ("PXF", buf, MB_SET);
	    sprintf (buf, "option seqno %d\0", sup->imgSeqNo);
	    mbusBcast ("SMCMgr", buf, MB_SET);

	    mbusBcast ("SMCMgr", "segList", MB_GET|MB_STATUS);
*/


            ndhsSetImIndex (sup->dca_tid, sup->imgSeqNo, sup->imgSeqNo);
            ndhsStartReadout (sup->dca_tid, sup->imgSeqNo, sup->fileRoot,
		imagetype);
            ndhsConfigureImage (sup->dca_tid, sup->imgSeqNo, sup->fileRoot,
		imagetype);
            ndhsSetExpID (sup->dca_tid, sup->imgSeqNo, expID);

	    break;

	} else if (strncmp (msg, "keywmon", 7) == 0) {

/*  NOT YET FULLY IMPLEMENTED -- Suspected memory corruption
**
** 	    supUpdateKeywDB (&msg[8]);
*/
	    break;

	} else if (strncmp (msg, "clean done", 10) == 0) {
	    /*  Update status of SMC.  We do this outside the normal polling
	    **  cycle to keep the display current.
	    */
	    expID = (double) atof (&msg[10]);
t2 = time((time_t)NULL);
/*
fprintf (stderr, "Time [%d] dca_done -> clean_done [expID = %.6lf]....\n",
    (t2-t1), expID);
*/
    	    pqSetStat (sup, expID, PQ_AVAILABLE);
	    mbusBcast ("SMCMgr", "segList", MB_GET|MB_STATUS);
	    e_time = time ((time_t)NULL);
	    dca_done++;
	    if (console)
	        fprintf (stderr, "SMC CLEAN: ExpID %.6lf (time=%d  done=%d)\n",
		    expID, (int)(e_time-s_time), dca_done);

t1 = time((time_t)NULL);

	    break;

	} else if (strncmp (msg, "process", 7) == 0) {
	    if (strncmp (&msg[8], "smc done", 8) == 0) {
	        /* Tell the picfeed procs to start processing.
	        */
		expID = (double) atof (&msg[17]);
		if (console)
		    fprintf (stderr, 
			"SMC PROCESS COMPLETE: ExpID %.6lf (npxf=%d of %d)\n\n",
			expID, npxf_started+1, sup->numProcMgrs);

		npxf_started++;
		if (npxf_started >= sup->numProcMgrs) {
		    sprintf (reply, "seqno %d\0", sup->imgSeqNo);
	            mbusBcast ("PXF", reply, MB_SET);
		    sprintf (reply, "dca_tid %d\0", sup->dca_tid);
	            mbusBcast ("PXF", reply, MB_START);

		    sprintf (reply, "process %.6lf\0", expID);
		    if (console)
		        fprintf (stderr, "PXF begin: %s\n", reply);
	            mbusBcast ("PXF", reply, MB_START);
		    npxf_started -= sup->numProcMgrs;
		}


	    } else if (strncmp (&msg[8], "pxf done", 8) == 0) {
		expID = (double) atof (&msg[17]);
		if (console)
		    fprintf (stderr, 
			"PXF PROCESS COMPLETE: ExpID %.6lf (npxf=%d of %d)\n\n",
			expID, npxf_finished+1, sup->numProcMgrs);

		npxf_finished++;
		if (npxf_finished >= sup->numProcMgrs) {
		    /* The PXF is done processing, i.e. transferring the
		    ** image to the DCA.  Send the EndReadout to close the
		    ** DCA processing, notify the SMCMgr of the image type
		    ** so it can save any sky buffers needed, and then clean
		    ** the image from the SMC.
		    */
/*
		    if (console)
		        fprintf(stderr,"ENDING READOUT: ExpID %.6lf\n\n",
			    expID);
		    ndhsEndReadout (sup->dca_tid, sup->imgSeqNo);
*/
		    npxf_finished -= sup->numProcMgrs;
		}

	    } else if (strncmp (&msg[8], "error", 5) == 0) {
	        /* Send an alert to the GUI. */

	    } else {
		if (console) {
	            fprintf (stderr, "\n***************************\n");
	            fprintf (stderr, "\n  UNKNOWN PROCESS CMD: '%s'\n", msg);
	            fprintf (stderr, "\n***************************\n");
		}
	    }


	} else {
	    clid = procFindClient (sup, -1, from); /* Get the client ID	*/
	    if (clid < 0 || procSetValue(sup, clid, msg, 1) < 0)
                fprintf (stderr, "Error setting process value.\n");
	}
	break;

    case MB_GET:
	if (IO_DEBUG)
	    fprintf (stderr, "GET on %s: '%s'  tid=%d\n", me, msg, from);

	clid = procFindClient (sup, -1, from);	/* Get the client ID	*/
	if (clid < 0)
	    break;
        if (procGetValue (sup, clid, msg, 1) < 0)
            fprintf (stderr, "Error getting process value.\n");
	break;

    case MB_STATUS:
	clid = procFindClient (sup, -1, from);	/* Get the client ID	*/
	if (clid < 0)
	    break;
        if (procUpdateStatus(sup, clid, msg, 1) < 0)
            fprintf (stderr, "Error updating status table.\n");
	break;

    case MB_ORPHAN:

	if (IO_DEBUG)
	    fprintf (stderr, "ORPHAN on %s: ", me);
	sscanf(msg, "Orphan {From: %d  To: %s  Subj: %d -- (%s)}",
	       &from_tid, &to_tid, &subj, txt);
	break;

    case MB_ACK:
	break;
    
    case MB_PING:

	if (IO_DEBUG)
	    fprintf (stderr, "PING on %s: %s\n", me, msg);
	break;

    case MB_DONE:                               /* Exit for now... */
        exit (0);
        break;

    case MB_ERR:

	if (IO_DEBUG)
	    fprintf (stderr, "ERR on %s: %s\n", me, msg);
	break;

    default:
	if (msg && IO_DEBUG) {
	    printf("Super recv:%d:  ", subject);
	    printf("   from:%d  subj:%d\n   msg='%s'\n", from, subject, msg);
	}
        if (msg && strncmp (msg, "quit", 4) == 0)
            exit (0);
    }
}


/*****************************************************************************
**  Log the message to the Supervisor GUI.
*/

logMessage (supDataPtr sup, int from, int to, char *msgtext)
{
    char *obmmsg = calloc (1, strlen (msgtext) + 128);

    sprintf (obmmsg, "setValue {%d %d { %s }}\0", from, to, msgtext);
    ObmDeliverMsg (sup->obm, "msglog", obmmsg);

    free ((char *)obmmsg);
}



/*****************************************************************************
**  Handle a command given to us from the command-line.
*/
inputEventHandler (char *cmd)
{
    char   c, host[64], buf[64], resp[32], val[12], config[128];
    char   cmdin[128], *tok = NULL, *group = NULL;
    char   *arg1=NULL, *arg2=NULL, *arg3=NULL;
    int    i, nread=0,  stid=0, ctid=0;


    /* Command loop */
    printf("zzdebug (%d)>  ", getpid());


    /*if (IO_DEBUG) fprintf (stderr, "cmd entered: '%s'\n", cmd); */

    bzero (cmdin, 128);
    strcpy(cmdin, cmd);
    if (cmd[0] == '?') {				/* HELP    */
        printf("Commands:\n");
        printf("    super [u|d]\t\tclient [u|d]\tspawn [c|s]\tstop [c|s]\n");
        printf("    init\t\thalt\t\tgetfds\tselect\n");
        printf("\n");

    } else if (strncmp(cmd, "init", 3) == 0) {		/* INIT    */
	printf("Initializing...");
	mbusInitialize(SUPERVISOR, NULL);
	printf("done.");

    } else if (strncmp(cmd, "super", 3) == 0) {		/* SUPER  */
	if (tok = mytok(cmd, 2)) {
	    if (tok[0] == 'u') {
		printf("Connecting as Supervisor...");
		stid = mbusConnect(SUPERVISOR, "super", TRUE);
		printf("Super:  stid = %d\n", stid);

	    } else if (tok[0] == 'd') {
		printf("Disonnecting as Supervisor...");
		mbusDisconnect(stid);

	    } else {
		printf("Invalid token '%s'....\n", tok);
	    }
	}

    } else if (strncmp(cmd, "client", 3) == 0) {	/* CLIENT  */
	if (tok = mytok(cmd, 2)) {
	    if (tok[0] == 'u') {
		printf("Connecting as Client\n");
		ctid = mbusConnect(CLIENT, "client", FALSE);
		if (ctid > 0) 
		    printf("Client:   tid = %d\n", ctid);
		else {
		    ctid = -ctid;
		    printf("Client running (%d), Supervisor not found\n",ctid);
		}

	    } else if (tok[0] == 'd') {
		printf("Disonnecting as Client...");
		mbusDisconnect(ctid);
	    }
	}

    } else if (strncmp(cmd, "spawn", 3) == 0) {		/* SPAWN   */
	if (tok = mytok(cmd, 2)) {
	    int child = -1;

	    if (tok[0] == 's') {
		printf("Spawning new Supervisor...\n");
		mbusSpawn("zzsup", NULL, NULL, &child);
		printf("Super:   tid = %d\n", child);

	    } else if (tok[0] == 'c') {
		printf("Spawning new Client...\n");
		mbusSpawn("zzclient", NULL, NULL, &child);
		printf("Client:   tid = %d\n", child);
	    }
	}

    } else if (strncmp(cmd, "ping", 3) == 0) {		/* PING   */
	for (i=0; i < 3; i++) {
	    /*
	    printf ("Super Ping = %d\n", mbusPing (mbAppGet(APP_STID), 100) );
	    */
	    printf ("Super Ping = %d\n", 
		mbusSend (SUPERVISOR, ANY, MB_PING, NULL) );
	}

    } else if (strncmp(cmd, "stop", 3) == 0) {		/* STOP   */
	if (tok = mytok(cmd, 2)) {
	    if (tok[0] == 's') {
		printf("Stopping Supervisor...\n");
	    } else if (tok[0] == 'c') {
		printf("Stopping Client...\n");
	    }
	}

    } else if (strncmp(cmd, "bcast", 3) == 0) {		/* BCAST  */
	if (group = mytok(cmd, 2))
	    mbusBcast (group, (arg1=mytok(cmdin,3)), MB_STATUS);

    } else if (strncmp(cmd, "send", 3) == 0) {		/* SEND   */
	if (tok = mytok(cmd, 2)) {
	    if (tok[0] == 's') {
		mbusSend(SUPERVISOR, ANY, MB_STATUS, (arg1=mytok(cmdin,3)));
	    } else if (tok[0] == 'c') {
		mbusSend(CLIENT, ANY, MB_STATUS, (arg1=mytok(cmdin,3)));
	    } else {
		printf ("Usage: send [s|c] <msg>\n");
	    }
	}


    } else if (strncmp(cmd, "whoami", 3) == 0) {	/* WHOAMI   */
	printf ("%s\n",  mbAppGetName());

    } else if (strncmp(cmd, "halt", 3) == 0) {		/* HALT    */
	printf("Halting message bus....\n");
	if (mbusHalt() < 0)
	    fprintf(stderr, "Error halting message bus....\n");

    } else if (strncmp(cmd, "getfds", 3) == 0) {	/* GETFDS  */
	int i, nfds, *fds;

	nfds = pvm_getfds(&fds);
	printf("nfds = %d\t", nfds);
	for (i = 0; i < nfds; i++)
	    printf("fd[%d]=%d  ", i, fds[i]);
	printf("\n");

    } else {
	printf ("unrecognized command: '%s'\n", cmd);
    }

    if (tok)   free (tok);
    if (group) free (group);
    if (arg1)  free (arg1);
    if (arg2)  free (arg2);
    if (arg3)  free (arg3);

    printf("\nzzdebug (%d)>  ", getpid());
}


/*****************************************************************************
**  Tokenize a command string.
*/
char *mytok(char *str, int N)
{
    register int i, ntok = abs(N);
    char *ip, buf[1024];

    if (!str) 
	return (NULL);
    else
	ip = strdup (str);

    while (isspace(*ip)) ip++;			/* skip leading whitespace */

    for (i = 0; *ip && i < (N - 1); i++) { 	/* find requested token    */
	while (!isspace(*ip)) ip++;
	while (isspace(*ip)) ip++;
    }
    ip--;

    strcpy (buf, ip);				/* terminate after token   */
    for (i=0; buf[i] != '\n' && !isspace(buf[i]); i++)
	;
    buf[i] = '\0';

    if (ip) free (ip);

    return ( strdup(buf) );
}



/*---------------------------------------------------------------------------
**  MBus Handlers -- These procedures are used to handle notification events
**  we get from the system.  For the moment we just print a status message,
**  a more formal method of updating the Supervisor state needs to be done.
**-------------------------------------------------------------------------*/

int supHostAdded (int mid)
{
    int n;
    pvm_unpackf( "%d", &n );
    printf ("%s: %d new hosts just added ***\n", mbAppGetName(), n);
}

int supTaskExited (int mid)
{
    int n;

    pvm_unpackf( "%d", &n );
    printf ("*** %s: task %d just exited ***\n", mbAppGetName(), n);
		
    sup_message (sup_g, "warning", "on");
}

int supHostDeleted (int mid)
{
    int n;
    pvm_unpackf( "%d", &n );
    printf ("*** %d new hosts just deleted ***\n", mbAppGetName(), n);
}
