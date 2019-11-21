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


struct timeval tv;
struct timezone tz;

int loop = 1;
int delay = 0;
int interactive = 0;

int procDebug;



int main(int argc, char *argv[])
{
    int k, expnum, nim, blkSize;
    int *p, *imAddr, szIm;
    int nelem, sockbufsize, size;
    int xs[4], ys[4], ncols, nrows;
    int totRowsPDet, totColsPDet, imSize, bytesPxl;
    int i, j, mytid, PanOffset = 0, use_nocs = 0;
    int from_tid, to_tid, subject;
    char resp[80], retStr[80], mdBuf[32776];
    char *op, *obsID = "TestID";
    char keyw[32], val[32], comment[64];
    char *host, *msg, cmd[80];
    long istat, sock;
    double expID;


    mdConfig_t mdCfg;
    fpConfig_t fpCfg;
    dhsHandle dhsID;



    /* Process the command-line arguments.
     */
    for (i = 1; i < argc; i++) {
	if (strncmp(argv[i], "-help", 5) == 0) {
	    /*clientUsage (); */
	    fprintf (stderr, "ERROR No Help Available.\n");
	    exit(0);;

	} else if (strncmp(argv[i], "-delay", 5) == 0) {
	    delay = atoi(argv[++i]);

	} else if (strncmp(argv[i], "-A", 2) == 0) {
	    PanOffset = 0;

	} else if (strncmp(argv[i], "-B", 2) == 0) {
	    PanOffset = 1;

        } else if (strncmp(argv[i], "-host", 5) == 0) {
            dcaInitDCAHost ();       		/* set simulated host */
            dcaSetSimHost (argv[++i], 1);

	} else if (strncmp(argv[i], "-nocs", 2) == 0) {
	    use_nocs = 1;

	} else if (strncmp(argv[i], "-debug", 5) == 0) {
	    procDebug = atoi(argv[++i]);

	} else if (strncmp(argv[i], "-interactive", 5) == 0) {
	    interactive++;
	    loop += 999;

	} else if (strncmp(argv[i], "-loop", 5) == 0) {
	    loop = atoi(argv[++i]);

	} else {
	    /*clientUsage (); */
	    fprintf (stderr, "ERROR Invalid arg: '%s'\n", argv[i]);
	    exit(0);;
	}
    }



    /* initialize global values */
    dhs.expID = 0;
    dhs.obsSetID = "";

    ncols = 2112;
    nrows = 2048;
    nelem = ncols * nrows;
    totColsPDet = ncols;
    totRowsPDet = nrows;

    szIm = totRowsPDet * totColsPDet;
    p = malloc(szIm * sizeof(int));

    /* Fill the array with a diagonal ramp.  Reference pixels will 
     ** indicate the image number.
     */
    for (i = 0; i < nrows; i++) {
	for (j = 0; j < ncols; j++) {
	    if (j >= (ncols - 64))
		p[i * ncols + j] = 4000 + expnum;	/* reference pixels */
	    else
		p[i * ncols + j] = i + j;
	}
    }


    bytesPxl = 4;
    imSize = totRowsPDet * totColsPDet * bytesPxl;
    xs[0] = 0;
    ys[0] = 0;
    xs[1] = 2 * ncols;
    ys[1] = 0;
    xs[2] = 0;
    ys[2] = 2 * nrows;
    xs[3] = 2 * ncols;
    ys[3] = 2 * nrows;


    /*  Simulate a SysOpen from the PAN.
     */
    istat = 0;
    printf("=================start dhsSysOpen=================\n");
    dhsSysOpen(&istat, resp, (dhsHandle *) & dhsID, (ulong) IamPan);
    if (istat != DHS_OK) {
	fprintf(stderr, "ERROR: dhsSysOpen failed. \\\\ %s\n", resp);
	exit(1);
    }
    printf("================= end dhsSysOpen =================\n");

    sockbufsize = 0;
    size = sizeof(int);


    printf("=================start dhsOpenConnect=================\n");
    (void) dhsOpenConnect(&istat, resp, (dhsHandle *) & dhsID,
			  IamNOCS, &fpCfg);
    if (istat < 0) {
	fprintf(stderr, "ERROR: dhsOpenConnect failed. \\\\ %s\n", resp);
	exit(1);		/* exit for now */
    }
    printf("=================end dhsOpenConnect=================\n");


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


    /* 
     */
    expID = 2454133.0;



    /*  AV Pair Header.  We create this for each image so we can verify
     **  we grabbed the right metadata pages, i.e. the keyword values will
     **  be "image_<expnum>_<keywnum>".  Likewise, the image array will
     **  have reference pixels that are "4000+<expnum>".
     */
    op = mdBuf;
    for (i = 1; i <= 256; i++) {
	memset(keyw, ' ', 32);
	memset(val, ' ', 32);
	memset(comment, ' ', 64);

	sprintf(keyw, "FOO_%04d", i);
	sprintf(val, "image_%04d_%04d", expnum, i);
	sprintf(comment, "comment string");

	memmove(op, keyw, 32);
	memmove(&op[32], val, 32);
	memmove(&op[64], comment, 64);
	op += 128;
    }
    blkSize = (size_t) (256 * 128);


    while (1) {

	/* We don't readlly care about the value, we just want a 
	 ** monotonically increasing number.....
	 */
	expID += 0.01;
	printf("expID::: %.6lf\n", expID);


	printf("\n\nHit <cr> to create a new image....\n");
	if (fgets(cmd, 80, stdin) == NULL || cmd[0] == 'q')
	    break;

	/* Send OpenExp from PAN
	 */
	printf("=================start dhsOpenExp=================\n");
	(void) dhsOpenExp(&istat, resp, (dhsHandle) dhsID,
			  &fpCfg, &expID, obsID);
	if (istat < 0) {
	    printf("ERROR: NOCS: dhsOpenExp failed. \\\\ %s\n", resp);
	    exit(1);		/* exit for now */
	}
	printf("=================end dhsOpenExp=================\n");


	/* Send Meta Data
	 */
	blkSize = (size_t) (256 * 128);
	printf("=================start dhsSendMetaData=================\n");
	dhsSendMetaData(&istat, retStr, (dhsHandle) dhsID,
			(char *) mdBuf, blkSize, &mdCfg, &expID, obsID);
	if (istat < 0) {
	    printf("ERROR: 1st dhsSendMetaData failed. \\\\ %s\n", retStr);
	    exit(1);		/* exit for now */
	}
	printf("=================end dhsSendMetaData=================\n");

	/* Send 2 arrays of nelem size  */
	nim = 2;
	for (i = 0; i < nim; i++) {

	    imAddr = p;

	    fpCfg.xStart = (long) xs[(PanOffset * 2) + i];
	    fpCfg.yStart = (long) ys[(PanOffset * 2) + i];
	    fpCfg.xSize = ncols;
	    fpCfg.ySize = nrows;
	    printf("Sending %d bytes,  istat=%d\n", imSize, istat);
	    printf("================start dhsSendPixelData================\n");
	    dhsSendPixelData(&istat, retStr, (dhsHandle) dhsID,
			     (void *) imAddr, (size_t) imSize,
			     &fpCfg, &expID, (char *) NULL);
	    if (istat < 0) {
		printf
		    ("ERROR: dhsSendPixelData failed: %d. \\\\ %s\n",
		     retStr);
		exit(1);
	    }
	    printf("=================end dhsSendPixelData=================\n");

	}

	/* Send 2nd MD Post header from PAN
	 */
	printf("=================start dhsSendMetaData=================\n");
	dhsSendMetaData(&istat, retStr, (dhsHandle) dhsID, (char *) mdBuf,
			blkSize, &mdCfg, &expID, obsID);
	if (istat < 0) {
	    printf("ERROR: 2nd dhsSendMetaData failed. \\\\ %s\n", retStr);
	    exit(1);		/* exit for now */
	}
	printf("=================end dhsSendMetaData=================\n");

	/* Send the Close Exposure message.
	 */
	printf("=================start dhsCloseExp=================\n");
	(void) dhsCloseExp(&istat, resp, (dhsHandle) dhsID, expID);
	if (istat < 0) {
	    printf("ERROR: panDataSaver: dhsCloseExp failed. \\\\ %s\n", resp);
	    exit(1);		/* exit for now         */
	}
	printf("=================end dhsCloseExp=================\n");
    }


    printf("=================start dhsCloseConnect=================\n");
    dhsCloseConnect(&istat, retStr, (dhsHandle) dhsID);
    printf("=================end dhsCloseConnect=================\n");

    printf("=================Closing COLLECTOR socket=================\n");
    close(sock);
    free(p);

    exit(0);
}
