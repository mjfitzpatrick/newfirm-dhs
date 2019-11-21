#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>

/*
#if !defined(_dhsUtil_H_)
#include "dhsUtil.h"
#endif
*/

/* These PRINT definitions are by default at /MNSN/soft_dev/inc/debug.h */

#define DPRINT(Level, Arg, Msg) \
 if(Arg >= Level) {fprintf(stderr, "\t*DBG* %s", Msg); fflush(stderr);}

#define DPRINTF(Level, Arg, Msg, Val) if(Arg >= Level) \
{   char __dbgMsg[256],__dbgMsg2[256];\
    sprintf(__dbgMsg, "\t*DBG* %s", Msg);\
    sprintf(__dbgMsg2, __dbgMsg, Val);\
    fputs (__dbgMsg2, stderr); \
    fflush(stderr);\
}

#if !defined(_FITSIO_H)
#include "fitsio.h"
#endif

#include "pxf.h"
#include "smCache.h"
#include "mbus.h"

extern smCache_t *smcOpen();
extern smcPage_t *smcGetPage();

smCache_t *smc    = (smCache_t *)NULL;
smcPage_t *page   = (smcPage_t *)NULL;
smcSegment_t *seg = (smcSegment_t *)NULL;

char *procPages	  = (char *)NULL;

                                        /* I/O Handlers                 */
int   mbusMsgHandler (int fd, void *client_data);

/*
void pxfFileOpen( XLONG *istat,char *resp,double *expID,fitsfile **fd);
*/

int	save_fits	= FALSE;	/* Output stream type		*/
int	save_mbus	= TRUE;		
					/* Message bus variables	*/
int	use_mbus	= TRUE;		
int	console		= FALSE;
int	verbose		= FALSE;
int	noop		= FALSE;
int	mb_tid, mb_fd;

int	dca_tid		= 0;
int	seqno		= 0;





main (int argc, char **argv) 
{
    char  c, fname[64], root[64], buf[64], savex[32], val[12], config[128];
    char resp[80];
    int	  i, j, k, debug=0, clear=1, nimages=1, delay=0, interactive=1;
    int   nsegs, pagenum, nvals,index, len, dirflg, fiflg;
    void  *data;
    char  *cdata, *ip;
    int   *idata;
    short *sdata;
    XLONG istat;
    double expID;
    XLONG lexpID, saveExpID;
    int fs=0;
    fitsfile *fd = (fitsfile *)NULL; 


    procDebug = 500;    /* Debug flag for DPRINT macro */

    if (argc == 2) {
       printf ("Usage: pxf -dir <Output FITS dir> -froot <FITS root name>\n");
       printf ("           -debug_level 'int >= 10'\n");
       return;
    }

    strcpy (pxfFILENAME, "image");
    strcpy (pxfDIR,"/home/data");

    dirflg=0; fiflg=0;
    for (i=1; i < argc; i++) {
        if (strncmp (argv[i], "-dir", 3) == 0) {
            strcpy(pxfDIR,  argv[++i]);
            dirflg=1;

        } else if (strncmp (argv[i], "-froot", 3) == 0) {
            strcpy(pxfFILENAME, argv[++i]);
            fiflg=1;

        } else if (strncmp (argv[i], "-console", 3) == 0) {
            console++;

        } else if (strncmp (argv[i], "-verbose", 3) == 0) {
            verbose++;

        } else if (strncmp(argv[i], "-host", 3) == 0) {
            mbInitMBHost();
            if ((argc - i) > 1 && (argv[i+1][0] != '-'))
                mbSetSimHost (argv[++i], 1);
            else {
                fprintf (stderr, "Error: '-host' requires an argument\n");
                exit (1);
            }

        } else if (strncmp (argv[i], "-debug", 3) == 0) {
            procDebug = atoi (argv[++i]);

        } else if (strncmp (argv[i], "-noop", 2) == 0) {
            noop = TRUE;

        } else if (strncmp (argv[i], "-mbus", 3) == 0) {
            use_mbus = TRUE;
            save_mbus = TRUE;
            save_fits = FALSE;

        } else if (strncmp (argv[i], "-proc", 3) == 0) {
	    procPages = argv[++i];

        } else if (strncmp (argv[i], "-fits", 3) == 0) {
            save_mbus = FALSE;
            save_fits = TRUE;

        } else if (strncmp (argv[i], "-nombus", 5) == 0) {
            use_mbus = FALSE;
            save_mbus = FALSE;
            save_fits = TRUE;

        } else {
            printf ("Unrecognized option: %s\n", argv[i]);
	    printf ("Usage: pxf -dir <Output dir> -froot <root name>\n");
	    printf ("           -debug_level 'int >= 10'\n");
	    return;
        }
    }
    
    sprintf (config, "debug=%d", debug);

    /*  Open/Attach to the cache.
     */
    if ((smc = smcOpen (config)) == (smCache_t *)NULL)
	fprintf (stderr, "Error opening cache, invalid file?.\n");


    if (use_mbus) {
        /* Open a connection on the message bus.
        */
        if ((mb_tid = mbusConnect("PXF", "PXF", FALSE)) <= 0) {
            fprintf (stderr, "ERROR: Can't connect to message bus.\n");
            exit (1);
        }

        if ((mb_fd = mbGetMBusFD()) >= 0) {     /* Add the input handlers. */
            mbusAddInputHandler (mb_fd, mbusMsgHandler, NULL);

        } else {
            fprintf (stderr, "ERROR: Can't install MBUS input handler.\n");
            exit (1);
        }
    }


    /*  Send initial status to the Supervisor for display.
    */
    if (use_mbus) {
	if (console)
            fprintf (stderr, "Waiting for input...\n");
        mbusSend (SUPERVISOR, ANY, MB_STATUS, "Waiting for data...");

	/*  Get the initial sequence number. 	*/
	/*  Get the initial Directory path.  	*/
	/*  Get the initial FName root. 	*/

        /*  Begin processing I/O events.  Note: This never returns....
        */
        mbusSend (SUPERVISOR, ANY, MB_READY, "READY PXF");
        mbusAppMainLoop (NULL);

    } else {

	/* If we're not using the message bus, the 'save_fits' flag has
	** already been set, so don't check for it here.  We'll never use
	** this task as a one-off feed to the message bus.
	*/

        smcSetSeqNo  (smc, 0);	/* Initialize sequence number		*/

        if (dirflg==0) {   	/* Get Dirname from share memory 	*/
            len = min(250, strlen((char *)smcGetDir(smc)));
            memmove (pxfDIR, (char *)smcGetDir(smc), len);
        }
        if (fiflg==0) {   	/* Get Filename from share memory 	*/
            len = min(250, strlen((char *)smcGetFRoot(smc)));
            memmove (pxfFILENAME, (char *)smcGetFRoot(smc), len);
        }
    
        pxfFLAG = 3;   /* indicates that DIR and FILENAME are defined. 	*/


        /*  Simulate the readout sequence
         */
        index = 1;  
        i = 0;
        savex[0]=' ';
        saveExpID = 0;
        strcpy(fname,pxfFILENAME);   /* save root filename */
        while ((page = smcNextPage (smc, 1000))) {
	    smcAttach (smc, page); 		/* Attach to the page. */
	    switch (page->type) { 		/* Process the data.   */
	    case TY_VOID:
	        procFITSData (page, fd);
	        break;
	    case TY_DATA:
	        procFITSData (page, fd);
	        break;
	    case TY_META:
	        expID = smcGetExpID(page);
	        lexpID = expID; 
                lexpID = (expID - lexpID)*100000;
                if (lexpID != saveExpID) {
                     if (fd != (fitsfile *)NULL) {
		        if ( fits_close_file(fd,&fs) ) {
		            DPRINTF(10,procDebug,
                                 "pxf: fits close failed, fitsStatus = %d", fs);
			    exit;
		        }
                     }
		     len = min(250, strlen((char *)smcGetPageDir(page)));
		     if (dirflg == 0 && len > 0)
		         memmove (pxfDIR, (char *)smcGetPageDir(page), len);
		     len = min(250, strlen((char *)smcGetPageFRoot(page)));
		     if (fiflg == 0 && len > 0) {
		         memmove (pxfFILENAME, (char *)smcGetPageFRoot(page),
			    len);
		     } 
 
	             pxfFileOpen (&istat, resp, &expID, smc, &fd); 
    	             DPRINTF(10,procDebug,"%s\n", resp);
                     saveExpID = lexpID;
                 }

	        procFITSMetaData (page, fd);
	        break;
	    default:
	        break;
	    }

	    i++;
	    smcMutexOn ();
	    smcDetach (smc, page,TRUE); 	/*  Detach from the page.  */
	    smcMutexOff ();
        }


        if ( fits_close_file(fd,&fs) ) {
    	    DPRINTF(10,procDebug,"pxf: fits close failed, fitsStatus = %d", fs);
	    exit;
        }

        /*
        if (smc && !interactive) 
	    smcClose (smc, clear);
        */
    }
}
