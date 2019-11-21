#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __POSIX__
#include <sys/select.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#endif
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stddef.h>
#include <unistd.h>

#include "smCache.h"
#include "mbus.h"
#include "smcmgr.h"

extern 	int console;

extern	int disp_enable, stat_enable, rotate_enable, idListing;
extern	int disp_frame, disp_stdimg, trim_ref;
extern  char *procPages;

extern	char *mbGetMBHost();

int	 skysub		= 0;		/* sky subtraction flag	*/
skyFrame *skyframe	= (skyFrame *) NULL;



/**************************************************************************
**  MBUSMSGHANDLER -- Standard client application message handler.
**
*/
int
mbusMsgHandler (int fd, void *data)
{
    char *from = NULL, *to = NULL, *host = NULL, *msg = NULL;
    int  from_tid, to_tid, subject;
    int  mytid = mbAppGet(APP_TID);


    to_tid = subject = -1;
    if (mbusRecv(&from_tid, &to_tid, &subject, &host, &msg) < 0) {
	if (console)
            fprintf (stderr, "Error in mbusRecv()\n");
        if (host) free ((void *) host);
        if (msg)  free ((void *) msg);
	return (ERR);
    }

    /* See if message was meant specifically for me.... */
    if (to_tid == mytid && subject != MB_ERR) {
	if (console)
            fprintf (stderr, "SENDTO:  from:%d  subj:%d msg='%s'\n",
		from_tid, subject, msg);
        myHandler (from_tid, subject, msg);

    } else if (to_tid < 0) {
        if (console && strcmp (msg, "no-op") != 0 && 
	    strcmp (msg, "segList") != 0)
                fprintf (stderr, "BCAST:  from:%d  subj:%d msg='%s'\n",
		    from_tid, subject, msg);
                
        myHandler (from_tid, subject, msg);

    } else {
	if (console) {
            fprintf(stderr,"Monitor...\n");
            fprintf(stderr,"   from:%d\n   to:%d\n   subj:%d\n",
		from_tid, to_tid, subject);
            fprintf(stderr,"   host:'%s'\n   msg='%s'\n", host, msg);
	}
    }

    if (host) free ((void *) host);
    if (msg)  free ((void *) msg);
    return (OK);
}



/*  Local Message Handler
 */
myHandler(int from, int subject, char *msg)
{
    int   from_tid, to_tid, subj;
    int   tid = 0, pid = 0, status;
    int   mytid = mbAppGet (APP_TID);
    char  *txt, *me, who[128], host[128], buf[SZ_LINE];
    double expID;

    extern smCache_t *smc;
    extern int seqno;


    me = (char *)mbAppGetName();

    switch (subject) {
    case MB_CONNECT:
	if (console)
	    fprintf (stderr, "    CONNECT on %s: %s\n", me, msg);

	mbParseConnectMsg (msg, &tid, &who, &host, &pid);

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
	mbAddTaskExitHandler (tid, myTaskExited);
	 */

	break;

    case (MB_GET|MB_STATUS):
	smPageStats (smc, -1);		/* list all pages, let GUI truncate */
	break;

    case MB_START:
	if (console) 
	    fprintf (stderr, "    START on %s: %s\n", me, msg);

	if (strncmp (msg, "process", 7) == 0) {
	    if (strncmp (&msg[8], "all", 3) == 0) {
	        /*smcProcAll (smc);*/
		;

	    } else if (strncmp (&msg[8], "next", 4) == 0) {
	        /*smcProcNext (smc);*/
		;

	    } else {
	        /*  Begin processing an image given the ExpID.
	        */
	        expID = atof (&msg[8]);
	        smcLockExpID (smc, expID, procPages);
	        smcProcess (smc, expID);
	        smcUnlockExpID (smc, expID, procPages);
	    }
	}
	break;

    case MB_FINISH:
	if (console) 
	    fprintf (stderr, "    FINISH on %s: %s\n", me, msg);

	if (console)
	    fprintf (stderr, "=========== FINISH EXPID: %s\n", &msg[6]);
	if (strncmp (msg, "ExpID", 5) == 0) {
	    expID = atof (&msg[6]);
	    if (console)
		fprintf(stderr, "=========== Deleting EXPID: %.6lf\n\n", expID);
	    smcDelExpID (smc, expID, procPages);

	    bzero (buf, SZ_LINE);
	    sprintf (buf, "clean done %s", &msg[6]);
            mbusSend (SUPERVISOR, ANY, MB_SET, buf);

/*
            mbusSend (SUPERVISOR, ANY, MB_SET, "clean done");
*/

            mbusSend (SUPERVISOR, ANY, MB_ACK, "");
            mbusSend (SUPERVISOR, ANY, MB_ACK, "");
            mbusSend (SUPERVISOR, ANY, MB_ACK, "");
	}
	break;

    case MB_SET:
        if (console && strncmp (msg, "no-op", 5) != 0) 
	    fprintf (stderr, "    SET on %s: %s\n", me, msg);

	if (strncmp (msg, "option disp_enable", 18) == 0) {
	    disp_enable = atoi (&msg[19]);

	} else if (strncmp (msg, "option stat_enable", 18) == 0) {
	    stat_enable = atoi (&msg[19]);

	} else if (strncmp (msg, "option rot_enable", 17) == 0) {
	    rotate_enable = atoi (&msg[18]);

	} else if (strncmp (msg, "option idListing", 16) == 0) {
	    idListing = atoi (&msg[17]);

	} else if (strncmp (msg, "option seqno", 12) == 0) {
	    seqno = atoi (&msg[13]);
	    smcSetSeqNo (smc, seqno);

	} else if (strncmp (msg, "option froot", 12) == 0) {
	    char *froot = &msg[14];
	    smcSetFRoot (smc, froot);

	} else if (strncmp (msg, "option dirname", 14) == 0) {
	    char *dir = &msg[16];
	    smcSetDir (smc, dir);

	} else if (strncmp (msg, "option frame", 12) == 0) {
	    disp_frame = atoi (&msg[14]);

	} else if (strncmp (msg, "option trim", 9) == 0) {
	    trim_ref = atoi (&msg[11]);

	} else if (strncmp (msg, "option stdimg", 13) == 0) {
	    disp_stdimg = atoi (&msg[15]);

        } else if (strncmp (msg, "no-op", 5) == 0) {
/*
            mbusSend (SUPERVISOR, ANY, MB_ACK, "");
*/
	    	;

        } else if (strncmp (msg, "reset", 5) == 0) {
	    smcReset (smc);

        } else if (strncmp (msg, "skysub", 6) == 0) {
	    skysub = atoi (&msg[7]);

        } else if (strncmp (msg, "imtype", 6) == 0) {
	    smProcImtype (smc, &msg[7]);

        } else if (strncmp (msg, "debug", 5) == 0) {
            /* Send it to everyone, let the app decide which
            ** messages means anything to them.
            */
	    smcSetDebug (&msg[6]);
	}
	break;


    case MB_EXITING:
	/*  If it's the supervisor disconnecting, .....
	 */
	if (console) 
	    fprintf (stderr, "    DISCONNECT on %s: %s\n", me, msg);

	break;

    case MB_STATUS:
	/* Send a Status response.... */
        mbusSend (SUPERVISOR, ANY, MB_STATUS, "Waiting for data...");
	break;

    case MB_PING:
	/* Return an ACK/STATUS to sender .... */
        mbusSend (SUPERVISOR, ANY, MB_ACK, "");
	break;

    case MB_ERR: 				/* No-op */
	break;

    case MB_DONE:                               /* Exit for now... */
        exit (0);
        break;

    default:
	if (console) {
	    fprintf (stderr, "!!! DEFAULT recv:%d:  ", subject);
	    fprintf (stderr, "   from:%d  subj:%d\n   msg='%s'\n",
		from, subject, msg);
	}
        if (strncmp (msg, "quit", 4) == 0)
            exit (0);

    }
}


/****************************************************************************
** SMPAGESTATS -- Report SMC page details to the Supervisor.
*/

#define SZ_SEGLN	120

smPageStats (smCache_t *smc, int npages)
{
    smcPage_t *p = (smcPage_t *) NULL;
    smcSegment_t *s = (smcSegment_t *) NULL;
    int   i, np = ((npages < 0) ? max(smc->top+2,smc->npages) : npages);
    char  ptype[12], *smc_ctime, *ip, host[128];
    char  lb[SZ_SEGLN], line[SZ_SEGLN], *msg;

    time_t clock = time(0);
    char   *time_str = ctime (&clock);


    int maxid, shmid, id;
    struct shmid_ds shmseg;
    struct shm_info shm_info;
    struct shminfo shminfo;
    struct ipc_perm *ipcp = &shmseg.shm_perm;


    maxid = shmctl (0, SHM_INFO, (struct shmid_ds *) &shm_info);

    /* Allocate space for the message.
    */
    if (np > 100) np = 100;
    msg = calloc (1, ((3*np) * SZ_SEGLN));

    /*
    bzero (host, 128);
    (void) gethostname (host, 128);
    for (ip=host; *ip && *ip != '.'; ip++)
	;
    *ip = '\0';
    */
    strcpy (host, mbGetMBHost());


    /*
    fprintf(stderr,"\t\t------ Shared Memory Segments --------\n\n");
    fprintf(stderr,"    Pages allocated:  %ld  resident:  %ld  swapped:  %ld\n",
	shm_info.shm_tot, shm_info.shm_rss, shm_info.shm_swp);
    fprintf(stderr,"         Cache File:  %s\n\n", smc->sysConfig.cache_path);
    */

    sprintf(msg, "segList %s \0", host);

    /* Format some interesting stats about the SMC on this machine.
    */
    strcat (msg, " { \0");

    sprintf (line, "{ ctime %s } \0", 
	smUtilTimeStr(smc->ctime)); 			strcat (msg, line);
    sprintf (line, "{ utime %s } \0", 
	time_str); 					strcat (msg, line);
    sprintf (line, "{ cacheFile %s } \0", 
	smc->sysConfig.cache_path); 			strcat (msg, line);
    sprintf (line, "{ memKey 0x%x } \0", 
	smc->memKey); 					strcat (msg, line);
    sprintf (line, "{ shmid 0x%x } \0", 
	smc->shmid); 					strcat (msg, line);
    sprintf (line, "{ size %d } \0", 
	smc->cache_size); 				strcat (msg, line);
    sprintf (line, "{ memUsed %.2f } \0", 
	(double)smc->mem_allocated/(1024.*1024.*1024.));strcat (msg, line);
    sprintf (line, "{ memAvail %.2f } \0", 
	(double)smc->mem_avail/(1024.*1024.*1024.)); 	strcat (msg, line);
    sprintf (line, "{ numProcs %d } \0", 
	smc->nattached); 				strcat (msg, line);
    sprintf (line, "{ nsegs %d } \0", 
	smc->npages);					strcat (msg, line);

    strcat (msg, " } \0");


    /* Begin formatting the segment list table.
    */
    strcat (msg, " {\0");
    if (!idListing) {
        strcat (msg, "    memKey     shmId    size     \0");
        strcat (msg, "  type finalr  nattch  status   ctime\n\0");
        strcat (msg, "    ------     -----    ----       \0");
        strcat (msg, "---- ------  ------  ------   -----\n\0");
    } else {
        strcat (msg, "    memKey      shmId    size       type     ExpID\n");
        strcat (msg, "    ------      -----    ----       ----     -----\n\n");
    }


    /*  Print some info on the SMC cache segment itself.
     */
    if (!idListing) {
        sprintf (line, "    0x%08x %-8d %-10d SMC  00%d0%d0  \0",
            smc->memKey, smc->shmid, smc->cache_size, 
	    smc->nattached, smc->vm_locked);
        strcat (msg, line);

        shmid = shmctl (smUtilKey2ID(smc->memKey), IPC_STAT, &shmseg);
        smc_ctime = smUtilTimeStr ((double)smc->ctime);
        sprintf (line, "%-6ld %s %s %8.8s\n\0", 
	    (unsigned long) shmseg.shm_nattch,
	    ipcp->mode & SHM_DEST ? "dest" : "    ",
	    ipcp->mode & SHM_LOCKED ? "lock" : "    ", &smc_ctime[11]);
        strcat (msg, line);
    } 


    /*  Now print each of the managed segments.
     */
    for (i=0; i < np; i++) {
      p = &smc->pdata[i];
      s = SEG_PTR(p);

      if (p->memKey) {
	id = smUtilKey2ID(p->memKey);
 	shmid = shmctl (id, IPC_STAT, &shmseg);

	/* Make a readable string of the segment type. */
	strcpy ((char *)&ptype, (char *)smcType2Str(p->type));
	bcopy (&ptype[3], &ptype, 7);

	if (!idListing) {
            sprintf (line, "%3d 0x%08x %-8d %-10d %-4s \0", 
	        i, p->memKey, id, p->size, ptype);
            strcat (msg, line);

            sprintf (line, "%d%d%d%d%d%d  \0",
                p->free, p->initialized, p->nattached, 
                SEG_ATTACHED(p), p->ac_locked, p->nreaders);
            strcat (msg, line);

            sprintf (line, "%-6ld %s %s \0", 
	        (unsigned long) shmseg.shm_nattch,
	        ipcp->mode & SHM_DEST ? "dest" : "    ",
                ipcp->mode & SHM_LOCKED ? "lock" : "    ");
            strcat (msg, line);

	    smc_ctime = smUtilTimeStr ((double)shmseg.shm_ctime);
            sprintf (line, "%8.8s\n\0", &smc_ctime[11]);
            strcat (msg, line);

	} else {
	    smcAttach (smc, p);
            sprintf (line, "%3d 0x%08x  %-8d %-10d %-7s  %.6lf\n", 
                i, p->memKey, id, p->size, ptype, smcGetExpID(p));
            strcat (msg, line);
	    smcDetach (smc, p, FALSE);
	}

      }
    }

    strcat (msg, "}\n\n");

    /* Send it to the Supervisor for display.
    */
    mbusSend (SUPERVISOR, ANY, MB_SET, msg);

    /* Now create a summary message indicating only ow many pages we're
    ** using on his machine.
    */
    sprintf(msg, "segCount %s %d \0", host, smc->npages);
    mbusSend (SUPERVISOR, ANY, MB_SET, msg);

    free ((char *) msg); 		/* free the message buffer	*/
}



/*******************************************************************************
**  Set the debug flags.  We'll let the SMCMgr handle this for all processes
**  running on this machine.
*/

smcSetDebug (char *msg)
{
    char *ip, *op, flag[32];
    int level;


    bzero (flag, 32);
    for (ip=msg, op=flag; *ip && !isspace(*ip); )
        *op++ = *ip++;
    level = atoi (++ip);

    if (strcmp (flag, "super") == 0) {
        smcDebugFile ("/tmp/SUP_DEBUG", level);
    } else if (strcmp (flag, "io") == 0) {
        smcDebugFile ("/tmp/IO_DEBUG", level);
    } else if (strcmp (flag, "socket") == 0) {
        smcDebugFile ("/tmp/SOCK_DEBUG", level);
    } else if (strcmp (flag, "sendrec") == 0) {
        smcDebugFile ("/tmp/SR_DEBUG", level);
    } else if (strcmp (flag, "conf") == 0) {
        smcDebugFile ("/tmp/CONF_DEBUG", level);
    } else if (strcmp (flag, "proc") == 0) {
        smcDebugFile ("/tmp/PROC_DEBUG", level);
    } else if (strcmp (flag, "queue") == 0) {
        smcDebugFile ("/tmp/QUEUE_DEBUG", level);
    } else if (strcmp (flag, "collector") == 0) {
        smcDebugFile ("/tmp/COL_DEBUG", level);
    } else if (strcmp (flag, "pxf") == 0) {
        smcDebugFile ("/tmp/PXF_DEBUG", level);
    } else if (strcmp (flag, "smc") == 0) {
        smcDebugFile ("/tmp/SMC_DEBUG", level);
    } else if (strcmp (flag, "mbus") == 0) {
        smcDebugFile ("/tmp/MB_DEBUG", level);
    } else if (strcmp (flag, "all") == 0) {
        smcDebugFile ("/tmp/SUP_DEBUG", level);
        smcDebugFile ("/tmp/IO_DEBUG", level);
        smcDebugFile ("/tmp/SOCK_DEBUG", level);
        smcDebugFile ("/tmp/SR_DEBUG", level);
        smcDebugFile ("/tmp/CONF_DEBUG", level);
        smcDebugFile ("/tmp/PROC_DEBUG", level);
        smcDebugFile ("/tmp/QUEUE_DEBUG", level);
        smcDebugFile ("/tmp/COL_DEBUG", level);
        smcDebugFile ("/tmp/PXF_DEBUG", level);
        smcDebugFile ("/tmp/SMC_DEBUG", level);
        smcDebugFile ("/tmp/MB_DEBUG", level);

    }

}


smcDebugFile (char *file, int value)
{
    extern int errno;
    int  fd;

    if (value) {
        if (access (file, W_OK) < 0)
            fd = open (file, O_WRONLY | O_CREAT);

    } else {

        if (access (file, F_OK) == 0)
            unlink (file);
    }
}


/*******************************************************************************
**  Set the debug flags.  We'll let the SMCMgr handle this for all processes
**  running on this machine.
*/

#define	SKY		"sky"

smProcImtype (smCache_t *smc, char *msg)
{
    register int i, j, expnum;
    double       expID = atof (msg);
    char         *ip, *colID, type[SZ_FNAME];
    smcPage_t    *page;
    smcSegment_t *seg = (smcSegment_t *) NULL; 


    /* Extract the image type.
    */
    bzero (type, SZ_FNAME);
    while (*ip && !isspace(*ip)) ip++;		/* skip expID		*/
    ip++;					/* skip trailing space	*/
    strcpy (type, ip);				/* copy remainder	*/

    /* See if we need to save the frame.
    */
    if (!skysub || strcasecmp (SKY, type) != 0) 
	return;

    if (console)
	fprintf (stderr, "Saving expid=%.6lf as skysub frame...\n");

    /* Allocate the first frame if needed.
    */
    if (!skyframe)
	skyframe = (skyFramePtr) calloc (1, sizeof (skyFrame));

    skyframe->expID = expID;


    /*  Find the data pages to process.
    */
    for (i=0; i < smc->top; i++) {
        page = &smc->pdata[i];


        if (page->memKey == (key_t)NULL)
          continue;

        /* See whether we're supposed to process the page or not.
        */
        if (procPages && strstr(procPages, page->colID) == NULL)
            continue;

        if (smcEqualExpID(page->expID,expID)) {

            smcAttach (smc, page);

            if (page->type == TY_DATA || page->type == TY_VOID) {
		/* Found a DATA page for the specified exposure ID and
		** we know this is a SKY image, so save the pixels for later
		** processing of other frames.
		*/
		seg    = SEG_ADDR(page);
		expnum = seg->expPageNum;
		colID  = &seg->colID[0];
    		strcpy (skyframe->obsSetID, seg->obsetID);
		for (j=0; j < MAX_SKY_FRAMES; j++) {
		    if (skyframe->frame[i].data) {
		        if (skyframe->frame[i].dsize != seg->dsize)
			    (void)realloc (skyframe->frame[i].data, seg->dsize);

			/* Match the colID and expnum, copy the pixels. */
		        if (skyframe->frame[i].expPageNum == expnum &&
			    strcasecmp (skyframe->frame[i].colID, colID) == 0) {
			        bcopy (skyframe->frame[i].data, seg->data, 
				    seg->dsize);
			    	skyframe->frame[i].dsize = seg->dsize;
				break;
			}

		    } else {
			/* Allocate space for the pixels save it.
			*/
			skyframe->frame[i].data = (int *)malloc (seg->dsize);

		        skyframe->frame[i].expPageNum = expnum;
			skyframe->frame[i].dsize = seg->dsize;
			strcpy (skyframe->frame[i].colID, colID);
			bcopy (skyframe->frame[i].data, seg->data, seg->dsize);
			break;
		    }
		}
	    }

            smcDetach (smc, page, FALSE);
	}
    }
}
