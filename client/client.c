/*
#if !defined(_sockUtil_H_)
#include "sockUtil.h"
#endif
*/
#if !defined(_dhsUtil_H_)
#include "dhsUtil.h"
#endif
#if !defined(_dhsImpl_H_)
#include "dhsImplementationSpecifics.h"
#endif

#include "dcaDhs.h"

#include <sys/time.h>


#ifndef OK
#define OK 	1
#endif
#ifndef IamPan
#define IamPan 	1
#endif
#ifndef IamNOCS
#define IamNOCS 2
#endif



/* Client socket program to send a large buffer to a listening server */
/*
struct PanConfig {
    ulong xSize,ulong ySize,ulong xStar,yStart,dataType
}
struct arrayConfig {
    ulong activeXSize,ulong activeYSize,ulong xStar,yStart,uint refcols,
    uint refRows,int xDir yDir
}
*/

#define CARD_FMT	"%-8s%20f%-46s"

struct timeval 	tv;
struct timezone tz;

int loop 	= 1;
int delay 	= 0;
int interactive	= 0;
int use_nocs	= 0;

extern int procDebug;



int main(int argc, char *argv[])
{
    int  k, expnum, nim, blkSize;
    int  *p, *imAddr, szIm;
    int  nelem, sockbufsize, size;
    int  xs[4], ys[4], ncols, nrows;
    int  totRowsPDet, totColsPDet, imSize, bytesPxl;
    int  i, j, iloop;
    char resp[80], retStr[80], mdBuf[32776];
    char *op, *obsID = "TestID", cmd[80];
    char keyw[32], val[32], comment[64];
    long  istat, sock;
    double expID;


    mdConfig_t mdCfg;
    fpConfig_t fpCfg;
    dhsHandle dhsID;
    dhsHandle nocs_dhsID;



    /* Process the command-line arguments.
     */
    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-help", 2) == 0) {
            /*clientUsage ();*/
            exit (0);;

        } else if (strncmp(argv[i], "-delay", 5) == 0) {
            delay = atoi (argv[++i]);

        } else if (strncmp(argv[i], "-debug", 5) == 0) {
            procDebug = atoi (argv[++i]);

        } else if (strncmp(argv[i], "-interactive", 5) == 0) {
            interactive++;
	    loop += 999;

        } else if (strncmp(argv[i], "-loop", 5) == 0) {
            loop = atoi (argv[++i]);

        } else if (strncmp(argv[i], "-no_nocs", 8) == 0) {
            use_nocs = 0;

        } else {
            /*clientUsage ();*/
            exit (0);;
        }
    }



    /* Initialize connections. To the supervisior  */
    /* dhsHandle is a structure */

    /* initialize global values */
    dhs.expID = 0;
    dhs.obsSetID = "";




    /*  Simulate a SysOpen from the NOCS.
    */
    if (use_nocs) {
        istat = 0;
printf ("=================start dhsSysOpen=================\n");
        dhsSysOpen (&istat, resp, (dhsHandle *) &nocs_dhsID, (ulong) IamNOCS);
printf ("=================end dhsSysOpen=================\n");
        if (istat != DHS_OK)
	    exit(0);
    }

    /*  From the implementation specific structure we pass
    **  a pointer of type dhsHandle to the dhs routines.
    */
    istat = 0;
printf ("=================start dhsSysOpen=================\n");
    dhsSysOpen (&istat, resp, (dhsHandle *) &dhsID, (ulong) IamPan);
printf ("=================end dhsSysOpen=================\n");
    if (istat != DHS_OK)
	exit(0);


    sockbufsize = 0;
    size = sizeof(int);

    ncols = 2112;
    nrows = 2048;
    nelem = ncols * nrows;
    totColsPDet = ncols;
    totRowsPDet = nrows;

    szIm = totRowsPDet * totColsPDet;
    p = malloc(szIm * sizeof(int));


    if (use_nocs) {
printf ("=================start nocs dhsOpenConnect=================\n");
        (void) dhsOpenConnect (&istat, resp, (dhsHandle *) &nocs_dhsID,
	    IamNOCS, &fpCfg);
printf ("=================end nocs dhsOpenConnect=================\n");
        if (istat < 0) {
            printf ("ERROR: NOCS: dhsOpenConnect failed. \\\\ %s\n", resp);
            exit(1);		/* exit for now */
        }
    }

printf ("=================start dhsOpenConnect=================\n");
    (void) dhsOpenConnect(&istat, resp, (dhsHandle *) &dhsID, IamPan, &fpCfg);
printf ("=================end dhsOpenConnect=================\n");
    if (istat < 0) {
        printf("ERROR: panDataSaver: dhsOpenConnect failed. \\\\ %s\n", resp);
        exit(1);		/* exit for now */
    }
    
    bytesPxl = 4;
    imSize = totRowsPDet * totColsPDet * bytesPxl;
    xs[0] = 0; 		ys[0] = 0;
    xs[1] = 2 * ncols; 	ys[1] = 0;
    xs[2] = 0; 		ys[2] = 2 * nrows;
    xs[3] = 2 * ncols; 	ys[3] = 2 * nrows;
    

    /* set up for an AV Pair header write
    */
    mdCfg.metaType = DHS_MDTYPE_AVPAIR;
    mdCfg.numFields = DHS_AVP_NUMFIELDS;
    mdCfg.fieldSize[0] = (ulong) DHS_AVP_NAMESIZE;
    mdCfg.fieldSize[1] = (ulong) DHS_AVP_VALSIZE;
    mdCfg.fieldSize[2] = (ulong) DHS_AVP_COMMENT;
    mdCfg.dataType[0] = (ulong) DHS_UBYTE;
    mdCfg.dataType[1] = (ulong) DHS_UBYTE;
    mdCfg.dataType[2] = (ulong) DHS_UBYTE;
    
    if (interactive) {
	printf ("\n\nHit <cr> to begin, 'p' to ping, 'q' to quit....\n");
	while (1) {
	    if (fgets (cmd, 80, stdin) == NULL)
	        break;
	    else if (cmd[0] == 'q')
	        return;
	    else if (cmd[0] == 'p') {
		printf ("Pinging Supervisor...");
		printf ("Pinging Collector...");
	    } else
	        break;
	}
    }

    /* 
     */
    expID = 2454100.0;

    for (expnum = 0; expnum < loop; expnum++) {

        /* Fill the array with a diagonal ramp.  Reference pixels will indicate
        ** the image number.
        */
        for (i = 0; i < nrows; i++) {
           for (j = 0; j < ncols; j++) {
              if (j >= (ncols-64))
                  p[i * ncols + j] = 4000+expnum;	/* reference pixels */
              else
                  p[i * ncols + j] = i + j;
           }
        }

        /*  AV Pair Header.  We create this for each image so we can verify
	**  we grabbed the right metadata pages, i.e. the keyword values will
	**  be "image_<expnum>_<keywnum>".  Likewise, the image array will
	**  have reference pixels that are "4000+<expnum>".
        */
        op = mdBuf;
        for (i = 1; i <= 256; i++) {
            memset (keyw, ' ', 32);
            memset (val, ' ', 32);
            memset (comment, ' ', 64);

            sprintf (keyw, "FOO_%04d", i);
            sprintf (val, "image_%04d_%04d", expnum, i);
            sprintf (comment, "comment string");

            memmove (op, keyw, 32);
            memmove (&op[32], val, 32);
            memmove (&op[64], comment, 64);
            op += 128; 
        }
        blkSize = (size_t) (256 * 128);
    
	/* We don't readlly care about the value, we just want a 
	** monotonically increasing number.....
	*/
        expID += 0.01;
        printf("expID::: %.6lf\n", expID);

	if (use_nocs) {
            fpCfg.xStart = (long) 0;
            fpCfg.yStart = (long) 0;
            fpCfg.xSize = 2 * ncols;
            fpCfg.ySize = 2 * nrows;
    
	    /* Send OpenExp from NOCS
	    */
printf ("=================start nocs dhsOpenExp=================\n");
            (void) dhsOpenExp(&istat, resp, (dhsHandle) nocs_dhsID,
		&fpCfg, &expID, obsID);
printf ("=================end nocs dhsOpenExp=================\n");
            if (istat < 0) {
                printf("ERROR: NOCS: dhsOpenExp failed. \\\\ %s\n",
            	   resp);
                exit(1);		/* exit for now */
            }
    
	    /* Send PRE META from the NOCS.
	    */
printf ("=================start nocs dhsSendMetaData=================\n");
            dhsSendMetaData(&istat, retStr, (dhsHandle) nocs_dhsID,
		(char *) mdBuf, blkSize/4, &mdCfg, &expID, obsID);
printf ("=================end nocs dhsSendMetaData=================\n");
            if (istat < 0) {
                printf("ERROR: NOCS 2nd dhsSendMetaData failed. \\\\ %s\n",
		    retStr);
                exit(1);		/* exit for now */
            }

	}


printf ("=================start dhsOpenExp=================\n");
        (void) dhsOpenExp(&istat, resp, (dhsHandle) dhsID, &fpCfg, &expID,
            		  obsID);
printf ("=================end dhsOpenExp=================\n");
        if (istat < 0) {
            printf("ERROR: panDataSaver: dhsOpenExp failed. \\\\ %s\n", resp);
            exit(1);				/* exit for now */
        }
    
        for (iloop = 0; iloop < 2; iloop++) {
    
            fpCfg.xStart = (long) xs[iloop-1];
            fpCfg.yStart = (long) ys[iloop-1];
            fpCfg.xSize = ncols;
            fpCfg.ySize = nrows;
    
    
            /* Send Meta Data
	    */
            blkSize = (size_t) (256 * 128);
printf ("=================start dhsSendMetaData=================\n");
            dhsSendMetaData(&istat, retStr, (dhsHandle) dhsID, (char *) mdBuf,
            		blkSize, &mdCfg, &expID, obsID);
printf ("=================end dhsSendMetaData=================\n");
            if (istat < 0) {
                printf("ERROR: 1st dhsSendMetaData failed. \\\\ %s\n", retStr);
                exit(1);		/* exit for now */
            }
    
            /* Send 2 arrays of nelem size  */
            nim = 2;
            for (i = 0; i < nim; i++) {
    
                imAddr = p;
    
                fpCfg.xStart = (long) xs[(iloop*2)+i];
                fpCfg.yStart = (long) ys[(iloop*2)+i];
                fpCfg.xSize = ncols;
                fpCfg.ySize = nrows;
                printf("Sending %d bytes,  istat=%d\n", imSize, istat);
printf ("=================start dhsSendPixelData=================\n");
                dhsSendPixelData(&istat, retStr, (dhsHandle) dhsID,
            		     (void *) imAddr, (size_t) imSize, &fpCfg,
            		     &expID, (char *) NULL);
printf ("=================end dhsSendPixelData=================\n");
                if (istat < 0) {
            	    printf("ERROR: dhsSendPixelData failed: %d. \\\\ %s\n",
            	       retStr);
            	    exit(1);
                }
    
            }
    
    
            /* Send 2nd MD Post header from PAN
	    */
printf ("=================start dhsSendMetaData=================\n");
            dhsSendMetaData(&istat, retStr, (dhsHandle) dhsID, (char *) mdBuf,
            		blkSize, &mdCfg, &expID, obsID);
printf ("=================end dhsSendMetaData=================\n");
            if (istat < 0) {
                printf("ERROR: 2nd dhsSendMetaData failed. \\\\ %s\n", retStr);
                exit(1);		/* exit for now */
            }

        }				/* END of iloop LOOP 	*/


printf ("=================start dhsCloseExp=================\n");
        (void) dhsCloseExp(&istat, resp, (dhsHandle) dhsID, expID);
printf ("=================end dhsCloseExp=================\n");
        if (istat < 0) {
            printf("ERROR: panDataSaver: dhsCloseExp failed. \\\\ %s\n", resp);
            exit(1);			/* exit for now 	*/
        }


	if (use_nocs) {
	    /* Send POST META from NOCS
	    */
printf ("=================start nocs dhsSendMetaData=================\n");
            dhsSendMetaData(&istat, retStr, (dhsHandle) nocs_dhsID,
		(char *) mdBuf, blkSize/4, &mdCfg, &expID, obsID);
printf ("=================end nocs dhsSendMetaData=================\n");
            if (istat < 0) {
                printf("ERROR: NOCS 2nd dhsSendMetaData failed. \\\\ %s\n",
		    retStr);
                exit(1);		/* exit for now */
            }

	    /* Send CloseExp from NOCS.  This is supposed to trigger the 
	    ** processing in the Supervisor.
	    */
printf ("=================start nocs dhsCloseExp=================\n");
            (void) dhsCloseExp(&istat, resp, (dhsHandle) nocs_dhsID, expID);
printf ("=================end nocs dhsCloseExp=================\n");
            if (istat < 0) {
                printf("ERROR: NOCS: dhsCloseExp failed. \\\\ %s\n", resp);
                exit(1);			/* exit for now 	*/
            }
	}



	if (interactive) {
	    printf ("\n\nHit <cr> to create a new image....\n");
	    if (fgets (cmd, 80, stdin) == NULL || cmd[0] == 'q')
		break;

	} else if (delay > 0) {
	    printf ("\n\n");
	    sleep (delay);
	}
    }				/* END of expnum LOOP  */
    
    
printf ("=================start dhsCloseConnect=================\n");
    dhsCloseConnect(&istat, retStr, (dhsHandle) dhsID);
printf ("=================end dhsCloseConnect=================\n");

    if (use_nocs)
        dhsCloseConnect(&istat, retStr, (dhsHandle) nocs_dhsID);

    free(p);
printf ("=================Closing COLLECTOR socket=================\n");
    close(sock);
    
    
    exit(0);
}
    
    
    
readHdr(char *file, char *blkAddr, size_t * size)
{
    int fd;
    
    fd = open(file, O_RDONLY);
    *size = read(fd, blkAddr, 256 * 80);
    printf("size: %d\n", *size);
}
