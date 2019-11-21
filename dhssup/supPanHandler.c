#if !defined(_sockUtil_H_)
#include "sockUtil.h"
#endif
#if !defined(_dhsUtil_H_)
#include "dhsUtil.h"
#endif
#if !defined(_dhsImpl_H_)
#include "dhsImplementationSpecifics.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <sys/un.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>
#include <unistd.h>		/* for close() */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#include "super.h"
#include "dcaDhs.h"

#define DEBUG		1

#define SZMSG    	120
#define RCVBUFSIZE 	1024	/* Size of receive buffer */

/*
*/
#define	USE_TRIGGER_HOST	1


extern int  console, dca_done;

static char *supHostLookup (supDataPtr sup, char *host);
static char *mType();
static void  superProcNext (supDataPtr sup);
static int   dbg_t (void);



/****************************************************************************
**  SUPERHANDLECLIENT -- Handle a client message received over the public
**  socket.
*/
void
superHandleClient (supDataPtr sup, int socket, char *msg)
{
    char dev[SZMSG], buffer[RCVBUFSIZE];	/* Buffer for echo string */
    char *phost, *chost, *m = NULL;
    int recvMsgSize;				/* Size of received message */
    int curlen = RCVBUFSIZE;
    int i, tot = 0, clid;
    int mtype, size;

    struct msgHeader *msgh;			/* msgHeader msgh   	*/
    fpConfig_t fpCfg;				/* focal plane config	*/
    clientProcPtr cp;				/* client process ptr	*/
    panConnPtr pc;				/* pan connection ptr 	*/
    panPairPtr pp;				/* pair table pointer	*/

    static FILE *dbg = (FILE *)NULL;
#ifndef USE_TRIGGER_HOST
    static int open_exp=0, close_exp=0;
#endif


    /* Receives message code */
    bzero(buffer, RCVBUFSIZE);
    bcopy(msg, buffer, SZMSG);


    msgh = (struct msgHeader *) buffer;

    mtype = msgh->type;

    if (sup->debug) {
	if (dbg == (FILE *)NULL && access("/tmp/MSG_DEBUG", R_OK) == 0)
	    dbg = fopen ("/tmp/dhsmsg.log", "a+");
	fprintf(stderr, "mtype %d: %s    (DCA_SET:%d, DCA_NOOP:%d)\n",
		mtype, (m=mType(mtype)), DCA_SET, DCA_NOOP);
	if (dbg) {
	    fprintf(dbg, "%s - %.6lf - fd=%2d  type=%8d (%08o): %s\n", 
		procTimestamp(), msgh->expnum, socket, mtype, mtype, m);
	    fflush (dbg);
	}
        if (m) free (m);
    }

    if (mtype & DCA_SET) {
    	if (sup->debug)
	    fprintf (stderr, "DCA_SET received\n");

	if (mtype & DCA_FP_CONFIG) {
    	    if (sup->debug)
	        fprintf (stderr, "DCA_SET FP_CONFIG\n");

	    size = sizeof(fpCfg);
	    if (dcaRecv(socket, (char *) &fpCfg, size) != size) {
		fprintf(stderr, "DCA_FP_CONFIG error \n");
	    }

    	    if (sup->debug) {
	        fprintf (stderr, "DCA_FP_CONFIG received: %d bytes \n",
		    recvMsgSize);
	        fprintf (stderr, "fpCfg.(x,y)Start: (%d,%d)\n",
		    fpCfg.xStart, fpCfg.yStart);
	        fprintf (stderr, "fpCfg.(x,y)Size: (%d,%d)\n",
		    fpCfg.xSize, fpCfg.ySize);
	        fprintf (stderr, "expID: %.6lf   obsetID: %s\n",
		    msgh->expnum, msgh->obset);
    	    }
	}

    } else if (mtype & DCA_GET) {
	int subtype = mtype & (~DCA_GET);

    	if (sup->debug)
	    fprintf (stderr, "DCA_GET  subtype=%d\n", subtype);

	switch (subtype) {
	case DCA_DCADEV:

	    /* Get the client ID, and from that the Collector node/port we
	    ** associated with the PAN making this request.
	    */
    	    bzero(dev, SZMSG);
	    if ((clid = procFindBySocket (sup, socket)) < 0) {
	        strcpy (dev, "localhost:4100");
	        if (dcaSend(socket, dev, SZMSG) != SZMSG)
	            fprintf (stderr, "DCADEV fallback sending error\n");
		return;
	    }

	    /* Loop over the pairing table, trying to find a PAN host that
	    ** matches the client making this request and returning the 
	    ** Collector address it was assigned in the config file.
	    */
	    cp = &sup->procTable[clid];
	    phost = cp->host;
	    for (i=0; i < sup->numPanPairs; i++) {
                pp = &sup->pairTable[i];    /* get machine pairing          */
fprintf (stderr, "%2d:  phost='%s' pair->phost='%s' pair->chost='%s'\n",
    i, phost, pp->phost, pp->chost);
		if (strcasecmp (phost, pp->phost) == 0) 
		    break;
	    }

            /*  Now find the Collector running on the paired host.
            */
            for (i=0; i < sup->numPanConns; i++) {
                pc = &sup->connTable[i];    /* get the pan connection       */
                cp = pc->collector;         /* get collector                */
/*
fprintf (stderr, "%2d:  col->host='%s' pair->phost='%s' pair->chost='%s'\n",
    i, cp->host, pp->phost, pp->chost);
fprintf (stderr, "  :  pair->sock = %d  col->port = %d   inUse=%d\n", 
    pp->socket, cp->port, pc->inUse);
*/
                if (strcasecmp (cp->host, pp->chost) == 0 && 
		  (pp->socket > 0 ? (cp->port == pp->socket) : 1)) {
		    if (pc->inUse <= 0) {
			char msg[128];
			bzero (msg, 128);

                        /* Matched the Collector host with the entry for the PAN
                        ** in the pairing table.  Note we assume the pairings
                        ** are unique in the table.
                        */
	    	        sprintf(dev, "%s:%d", supHostLookup (sup, cp->host),
			    cp->port);

fprintf (stderr, "DCADEV sending Collector: %s, on socket: %d\n", dev, socket);
    	    	        if (sup->debug) {
	        	    fprintf (stderr,
		  	        "DCADEV sending Collector: %s, on socket: %d\n",
		  	        dev, socket);
		        }
	    	        if (dcaSend(socket, dev, SZMSG) != SZMSG)
	        	    fprintf (stderr, "DCADEV sending error\n");
		        return;
		    }
	        }
	    }

	    /* If we made it this far we didn't find a match....
	    */
    	    if (sup->debug)
	        fprintf (stderr, "DCADEV:  No Collector available, closing\n");

	    /* Get the client ID.
	    */
	    {
	        int clid;
    	        clientProcPtr cp;

	        if ((clid = procFindBySocket (sup, socket)) < 0) {
    	            if (sup->debug) 
			fprintf (stderr, "DCADEV bad client ID\n");
		    return;
	        } else {
	            cp = &sup->procTable[clid];
	            sup_disconnectClient (&cp->chan);
	        }
	    }
	    break;

	default:
	    fprintf (stderr, "Warning: invalid GET msg subtype: %d\n", subtype);
	    break;
	}


    } else if (mtype & DCA_NOOP) {
	/* Send an ACK in response.
	 */
    	if (sup->debug)
	    fprintf (stderr, "DCA_NOOP ...Sending ACK\n");
	msgh->type = DCA_ACK;
	msgh->whoami = DCA_SUPERVISOR;
	if (dcaSend(socket, (char *) msgh, SZMSG) != SZMSG)
	    fprintf (stderr, "DCA_NOOP error\n");

    } else if (mtype & DCA_OPEN) {
	clientProcPtr cp;
	char  msg[128];

 	bzero (msg, 128);
	if (mtype & DCA_EXP) {
            register int i, clid;
    	    panConnPtr pc;

	    /* Get the client ID.
	    */
	    if ((clid = procFindBySocket (sup, socket)) < 0)
		return;
    	    cp = &sup->procTable[clid];

    	    if (1||sup->debug) 
		fprintf (stderr, "%d: DCA_EXP OPEN received on sock %d, host=%s\n",
		    dbg_t(), socket, cp->host);

	    if (procUpdateStatus (sup, clid, "active", 1) < 0)
		fprintf (stderr, "Error updating status table.\n");
    	    if (sup->debug) fprintf (stderr, "from clid = %d\n", clid);

            sprintf (msg, "%s 1", cp->host);
            sup_message (sup, "expStat", msg);
fprintf (stderr, "DCA_OPEN EXP: '%s'\n", msg);

	    /*  Reset the transfer count on the connection. First,
            **  find the this PAN client host in the connection table.
            */
            for (i=0; i < sup->numPanConns; i++) {
                pc = &sup->connTable[i];
                if (pc->pan == cp) {
                    pc->bytesTransferred = 0;
		    break;
                }
            }

#ifdef USE_TRIGGER_HOST
	    if (isTriggerHost (sup, clid)) {
#else
	    if ((open_exp++) % 2) {
#endif
		if (sup->debug)
		    fprintf (stderr, "Adding to QUEUE: %.6lf\n", msgh->expnum);
		pqAdd (sup, msgh->expnum);
	    }

	    /* Clear the RTD status display for the new exposure.
	    */
	    if (sup->rtd.stat_enable)
                sup_message (sup, "rtdStat", "clear");
	    else
                sup_message (sup, "rtdStat", "sclear");


	} else {
            register int clid;

    	    if (sup->debug) 
		fprintf (stderr, "DCA_CONNECT OPEN received\n");

	    /* Get the client ID.
	    */
	    if ((clid = procFindBySocket (sup, socket)) < 0)
{
fprintf (stderr, "DCA_OPEN CONNECT: clid = %d\n", clid);
		return;
}
    	    cp = &sup->procTable[clid];

            sprintf (msg, "%s 1", cp->host);
            sup_message (sup, "conStat", msg);
fprintf (stderr, "DCA_OPEN CONNECT: '%s'\n", msg);
	    ;
	}


    } else if (mtype & DCA_CLOSE) {
	char  msg[128];

 	bzero (msg, 128);
	if (mtype & DCA_EXP) {
	    int clid;
    	    clientProcPtr cp;


	    /* Get the client ID.
	    */
	    if ((clid = procFindBySocket (sup, socket)) < 0) {
    	        if (sup->debug) fprintf (stderr, "EXP_CLOSE bad client ID\n");
		return;
	    }
    	    cp = &sup->procTable[clid];

    	    if (1||sup->debug) 
		fprintf (stderr, "%d: DCA_EXP CLOSE received on sock %d, host=%s\n",
		    dbg_t(), socket, cp->host);

	    if (procUpdateStatus (sup, clid, "inactive", 1) < 0)
		fprintf (stderr, "Error updating status table.\n");

    	    /* See if this is the trigger host.
	    */
#ifdef USE_TRIGGER_HOST
	    if (isTriggerHost (sup, clid)) {
#else
	    if ((close_exp++) % 2) {
#endif
		if (console || sup->debug)
		    fprintf (stderr, "READY in QUEUE: %.6lf\n", msgh->expnum);

		pqSetStat (sup, msgh->expnum, PQ_READY);
                sup_message (sup, "dataFlag", "waiting 1");
	    }

            sprintf (msg, "%s 0", cp->host);
fprintf (stderr, "DCA_CLOSE EXP: '%s'\n", msg);
            sup_message (sup, "expStat", msg);

	} else {
	    int clid;
    	    clientProcPtr cp;


    	    if (sup->debug) 
		fprintf (stderr, "DCA_CONNECT CLOSE received\n");

	    /* Get the client ID.
	    */
	    if ((clid = procFindBySocket (sup, socket)) < 0) {
    	        if (sup->debug) fprintf (stderr, "CON_CLOSE bad client ID\n");
		return;
	    } else {
	        cp = &sup->procTable[clid];
	        sup_disconnectClient (&cp->chan);
	    }

            sprintf (msg, "%s 0", cp->host);
fprintf (stderr, "DCA_CLOSE CONNECT: '%s'\n", msg);
            sup_message (sup, "conStat", msg);
	}


    } else {
    	if (sup->debug)
	    fprintf(stderr, "handling default %d: %s\n", mtype,
	    (m=mType(mtype)));
        if (m) free (m);
	msgh->type = DCA_FAIL;
	msgh->whoami = DCA_SUPERVISOR;
    }
}


static int 
dbg_t ()
{
    time_t t = time((long)0);
    return ((int) ((long)t  - (3L * 315532800L) - 220903200L));
}


/* Lookup the given host name in the pseudo-host table and return the IP
** address.  We return the input host name if not found.
*/
static char *
supHostLookup (supDataPtr sup, char *host)
{
    int i;
    hostTablePtr ht;

    for (i=0; i < sup->numHostTable; i++) {
	ht = &sup->hosts[i];
        if (strcmp (host, ht->name) == 0)
	    return (ht->ip_addr);
    }

    return (host);
}


#ifdef USE_TRIGGER_HOST
/* Determine whether the machine sending the message is the 'trigger'
** machine named in the config file.
*/
isTriggerHost (supDataPtr sup, int clid)
{
    clientProcPtr cp;				


    /* See if this is the trigger host.
    */
    cp = &sup->procTable[clid];
    if (cp) {
	char *ip, host[SZ_FNAME];

	bzero (host, SZ_FNAME);
	if (gethostname (host, SZ_FNAME) < 0)
	    strcpy (host, "localhost");
	for (ip=host; *ip; ip++) { 			/* strip domain */
	    if (*ip == '.') { *ip = '\0'; break; }
	}
	*ip = '\0';

fprintf (stderr, "** TriggerHost: '%s'  curhost: '%s'  procHost: '%s' --> ",
sup->triggerHost, host, cp->host);
	if ((strcasecmp (sup->triggerHost, "localhost") == 0 && 
	     strcasecmp (host, cp->host) == 0)  ||
            (strcasecmp (sup->triggerHost, cp->host) == 0)) {
fprintf (stderr, "MATCHED\n");
    	   	return (1);
	}
    }

fprintf (stderr, "NO MATCH\n");
    return (0);
}
#endif


/*  superProcNext -- Process the next ready exposure.
*/

static void
superProcNext (supDataPtr sup)
{
    double expID;
    extern double pqNext();


    /* Tell the SMC Managers on each node to begin processing the given
    ** exposure.  The queue will return a zero-valued expID if there are
    ** no xposures in the queue ready for processing.
    **
    ** When that is done the SMCMgr will send back a message used to trigger
    ** the PXF to begin processing.
    */
    expID = pqNext (sup);
    if (console && expID > 0.0)
        fprintf (stderr, "superProcNext:  expID = %.6lf, stat=%d, count=%d\n",
            expID, sup->qFirst->status, sup->qCount);

    if (expID > 0.0 && dca_done) {
        char   msg[SZ_LINE];
	extern time_t s_time;


        pqSetStat (sup, expID, PQ_ACTIVE);

        sprintf (msg, "process %.6lf\0", expID);
        mbusBcast ("SMCMgr", msg, MB_START);
        sprintf (msg, "%lf test", expID);
        sup_message (sup, "transStat", msg);

        sup->imgSeqNo++;
        s_time = time ((time_t)NULL);
        if (console)
            fprintf (stderr, 
		"superProcNext: STARTING expID = %.6lf, seqno=%d time=%d\n",
                expID, sup->imgSeqNo, s_time);
        sprintf (msg, "seqno %d\0", sup->imgSeqNo);
        mbusBcast ("PXF", msg, MB_SET);
        sprintf (msg, "option seqno %d\0", sup->imgSeqNo);
        mbusBcast ("SMCMgr", msg, MB_SET);

        sprintf (msg, "process %.6lf\0", expID);
        mbusBcast ("SMCMgr", msg, MB_START);
        dca_done = 0;
    }
}


/*  Utility routine to convert an integer message type to a printable string.
**  Note we leak memory from the strdup() so this should only be used when
**  debugging!
*/
static char *
mType (int type)
{
    char buf[128];

    bzero(buf, 128);
    strcpy(buf, "|\0");

    if (type & DCA_SET)
	strcat(buf, "DCA_SET:");
    if (type & DCA_GET) {
	strcat(buf, "DCA_GET:");
        if (type & DCA_DCADEV)
	    strcat(buf, "DCA_DCADEV:");
        return (strdup(buf));
    }

    if (type & DCA_OPEN)
	strcat(buf, "DCA_OPEN:");
    if (type & DCA_CLOSE)
	strcat(buf, "DCA_CLOSE:");
    if (type & DCA_READ)
	strcat(buf, "DCA_READ:");
    if (type & DCA_WRITE)
	strcat(buf, "DCA_WRITE:");
    if (type & DCA_INIT)
	strcat(buf, "DCA_INIT:");
    if (type & DCA_FAIL)
	strcat(buf, "DCA_FAIL:");
    if (type & DCA_NOOP)
	strcat(buf, "DCA_NOOP:");
    if (type & DCA_ALIVE)
	strcat(buf, "DCA_ALIVE:");
    if (type & DCA_SYS)
	strcat(buf, "DCA_SYS:");
    if (type & DCA_CONNECT)
	strcat(buf, "DCA_CONNECT:");
    if (type & DCA_CON)
	strcat(buf, "DCA_CON:");
    if (type & DCA_EXP)
	strcat(buf, "DCA_EXP:");
    if (type & DCA_META)
	strcat(buf, "DCA_META:");
    if (type & DCA_STICKY)
	strcat(buf, "DCA_STICKY:");
    if (type & DCA_PIXEL)
	strcat(buf, "DCA_PIXEL:");
    if (type & DCA_OBS_CONFIG)
	strcat(buf, "DCA_OBS_CONFIG:");
    if (type & DCA_FP_CONFIG)
	strcat(buf, "DCA_FP_CONFIG:");
    if (type & DCA_MD_CONFIG)
	strcat(buf, "DCA_MD_CONFIG:");
    if (type & DCA_EXPID)
	strcat(buf, "DCA_EXPID:");
    if (type & DCA_OBSID)
	strcat(buf, "DCA_OBSID:");
    if (type & DCA_EXP_OBSID)
	strcat(buf, "DCA_EXP_OBSID:");
    if (type & DCA_DIRECTORY)
	strcat(buf, "DCA_DIRECTORY:");
    if (type & DCA_FNAME)
	strcat(buf, "DCA_FNAME:");
    if (type & DCA_DCADEV)
	strcat(buf, "DCA_DCADEV:");

    return (strdup(buf));
}
