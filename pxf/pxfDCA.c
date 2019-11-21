#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <time.h>
#include <errno.h>
#include <pvm3.h>

#if !defined(_dhsUtil_H_)
#include "dhsUtil.h"
#endif

#include "pxf.h"
#include "ndhs.h"
#include "mbus.h"
#include "smCache.h"

#define SZ_VALSTR       80
#define SZ_CMDSTR       512
#define MAX_KWGROUPS    64
#define MAX_CARDS       512
#define MAX_IGNORED     10
#define MAX_HDRFILES    16
#define MAX_DIRFILES    512
#define HDRFILE_WAIT    3               /* wait for hdrfile creation */
#define MAX_WAIT        15              /* max hdrfile wait, seconds */

#ifdef SZ_FNAME
#undef SZ_FNAME
#endif
#define SZ_FNAME        256

#ifdef SZ_PATHNAME
#undef SZ_PATHNAME
#endif
#define SZ_PATHNAME     512


/*
#define	CTIO		1
*/
#define	KPNO		1


extern int console, verbose;
extern int procDebug;
extern int noop;

extern char *procPages;


/* Global data for conversion. */
static int picnum;
static int nimages;
static int bitpix;
static int naxis;
static int nsync;

#define   MB_TIMEOUT   5;		/* Message bus timeout (seconds). */


static struct timeval mb_timeout;
#define SET_TIMEOUT(tm,sec)  ((tm)->tv_sec = (sec), (tm)->tv_usec = 0)


static char *procDCAMoveStr (char *instr, int len);


extern int seqno, dca_tid;

extern int errno;
extern char *getenv();



procDCAMetaData (smcPage_t *p)
{
    int   i, j, s, nkw, nsend, nkeyw, data_size, nmatch;
    char *cdata, *ip, *edata, buf[SZ_LINE];
    smcSegment_t *seg = (smcSegment_t *) NULL; 
    mdConfig_t *mdCfg = smcGetMDConfig (p);
    fpConfig_t *fpCfg = smcGetFPConfig (p);
    double expID;
    char obsetID[80], kwdb[80], *keyw, *val, *comment, type[3];
    char *kmonMsg = (char *)NULL;
    XLONG istat;

#ifdef DO_TIMINGS
    time_t	t1, t2;
#endif


#ifdef FAKE_READOUT
	return;
#endif


    seg = SEG_ADDR(p);
    data_size = seg->dsize;

    /* See whether we're supposed to process the page or not.
    */
    if ( (procPages && strstr(procPages, seg->colID) == NULL) ||
	 (p->type != TY_META) )
	     return;

#ifdef DO_TIMINGS
    t1 = time ((time_t)NULL);
#endif

    expID = smcGetExpID(p);
    strcpy (obsetID,smcGetObsetID(p));
    cdata = ip = (char *) smcGetPageData (p);
    edata = cdata + data_size;

    nkeyw = data_size / 128;		/* AV Pairs = 128 bytes/card	*/

    /* Generate the keyword database name.
    */
    bzero (kwdb, 80);
    if (seg->expPageNum == 0)
	sprintf (kwdb, "%s_Pre", seg->colID);
    else
	sprintf (kwdb, "%s_Post", seg->colID);

    if (console) {
        fprintf (stderr, "Proc META : 0x%x size=%d nkeyw=%d\n",
		seg, data_size, nkeyw);
        fprintf (stderr, "          : ExpID=%.6lf ObsID=%s typ=%d\n",
	    expID, obsetID, p->type);
        fprintf (stderr, "          : mdCfg: type=%d  nfields=%d\n",
	    mdCfg->metaType, mdCfg->numFields);
        fprintf (stderr, "          : fpCfg: Start: %d,%d  Size: %d,%d\n", 
            fpCfg->xStart, fpCfg->yStart, fpCfg->xSize, fpCfg->ySize);
        fprintf (stderr, "          : page: colID='%s'  expPageNum=%d\n",
	    seg->colID, seg->expPageNum);
        fprintf (stderr, "          : seqno: %d  dca_tid: %d  kwdb: '%s'\n\n",
	    seg->seqno, dca_tid, kwdb);
    }

    ip = cdata;                           /* initialize                 */
    edata = (cdata + data_size);          /* find addr of end of data   */


    /* Initialize the keyword monitoring message.
    */
    if (NKeywords) {
        kmonMsg = calloc (1, NKeywords * SZ_LINE);
        sprintf (kmonMsg, "keywmon %.6lf ", expID);
	nmatch = 0;
    }

    for (nkw=0; nkw < nkeyw;) {

        /* Initialize the WriteHeaderData request for the keyword group.
        */
	seqno = seg->seqno;
	nsend = min((nkeyw-nkw), 16);
        pvm_initsend (PvmDataDefault);
        pvm_pkint (&seqno, 1, 1);
        pvm_pkstr (kwdb);
        pvm_pkint (&nsend, 1, 1);


        for (i=0; i < 16 && nkw < nkeyw; i++) {
	    keyw    = procDCAMoveStr (ip,      32);
	    val     = procDCAMoveStr (&ip[32], 32);
	    comment = procDCAMoveStr (&ip[64], 64);

            pvm_pkstr (keyw);		/* pack the strings		*/
            pvm_pkstr (val);
            pvm_pkstr ("S");
            pvm_pkstr (comment);

	    if (procDebug > 100 && nkw < 16)
    	        fprintf (stderr, "%2d: '%s' '%s' '%s'\n", 
		    nkw,keyw,val,comment);

/*
	    if (NKeywords && inKeywList (kwdb, keyw)) {
    		sprintf (buf, "%s.%s='%s' \0", kwdb, keyw, val);
		strcat (kmonMsg, buf);
		nmatch++;
	    }
*/

	    free (keyw);
	    free (val);
	    free (comment);

	    ip += 128;
	    nkw++;
        }

	if (console && verbose)
	    fprintf (stderr, "Sending WriteHeaderData for %s (%d) (%d) (%d)\n",
    		kwdb, i, nkw, nsend);

        pvm_send (dca_tid, DCA_WriteHeaderData);

        /* Send Sync.
        pvm_initsend (PvmDataDefault);
        pvm_pkint (&seqno, 1, 1);
        pvm_send (dca_tid, DCA_Synchronize);
        mb_timeout.tv_sec = MB_TIMEOUT;
        mb_timeout.tv_usec = 0;
        if ((s=pvm_trecv (dca_tid, DCA_Synchronize, &mb_timeout)) <= 0) {
	    if (console) {
                fprintf (stderr, "pxf: %s", (s == 0) ? 
                     "timeout waiting for response from DCA\n" :
                     "failed to synchronize with DCA");
            }
        }
        */

    }
    if (console) fprintf (stderr, "DONE Processing META : sent=%d\n", nkw);


    /* Send the requested keywords to the Supervisor for display.
    */
    if (NKeywords && nmatch)
	mbusSend (SUPERVISOR, ANY, MB_SET, kmonMsg);

    if (kmonMsg) free (kmonMsg);	/* free space			*/

#ifdef DO_TIMINGS
    t2 = time ((time_t)NULL);
    fprintf (stderr, "Time [%d] procDCAMetaData()\n", (t2-t1));
#endif
}


/*  Copy a string from an AVP buffer to a value, stripping leading and 
**  trailing whitespace in the process.  Both 'instr' and 'outstr' are 
**  assumed to be at least 'len' chars long.
*/

#define SZ_AVP_BUF	128

static char *
procDCAMoveStr (char *instr, int len)
{
    char *ip, buf[SZ_AVP_BUF];
    int i = len;

    
    bzero (buf, SZ_AVP_BUF);			/* zero buffer		*/
    bcopy (instr, buf, len);			/* move the string 	*/

						/* trim trailing space	*/
    for (i=strlen(buf)-1; (buf[i] == '\'' || isspace(buf[i])) && i > 0; )
	buf[i--] = '\0';					
    if (buf[i] == '\'' || isspace(buf[i]))
	buf[i--] = '\0';					
						/* skip leading space	*/
    for (ip=&buf[0], i=0; (*ip == '\'' || isspace(*ip)) && i++ < len; ip++)
	;					

    return (strdup(ip));			/* return final string 	*/
}


procDCAData (smcPage_t *p)
{
    int   i, j, data_size, nx, ny, nchunks=64, row=0, val, datalen;
    int   *idata, *ip, *op, swapped=0, nstreams=1, s, nbytes = 0;
    smcSegment_t *seg = (smcSegment_t *) NULL; 
    mdConfig_t *mdCfg = smcGetMDConfig (p);
    fpConfig_t *fpCfg = smcGetFPConfig (p);
    double expID;
    char obsetID[80];
    XLONG istat;
    DCA_Stream st;
#ifdef DO_TIMINGS
    time_t  t1, t2;
#endif


#ifdef FAKE_READOUT
	return;
#endif


    seg = SEG_ADDR(p);
    data_size = seg->dsize;

    /* See whether we're supposed to process the page or not.
    */
    if ( (procPages && strstr(procPages, seg->colID) == NULL) ||
	 (p->type == TY_META) )
	     return;

#ifdef DO_TIMINGS
    t1 = time ((time_t)NULL);
#endif
    expID = smcGetExpID(p);
    strcpy (obsetID,smcGetObsetID(p));
    idata = ip = (int *) smcGetPageData (p);

    if (console) {
        fprintf (stderr, "Proc DATA : 0x%x size=%d\n", seg, data_size);
        fprintf (stderr, "          : ExpID=%.6lf ObsID=%s seqno=%d\n",
	    expID, obsetID, seqno);
        fprintf (stderr, "          : mdCfg: type=%d  nfields=%d\n",
	    mdCfg->metaType, mdCfg->numFields);
        fprintf (stderr, "          : fpCfg: Start: %d,%d  Size: %d,%d\n", 
            fpCfg->xStart, fpCfg->yStart, fpCfg->xSize, fpCfg->ySize);
        fprintf (stderr, "          : page: colID='%s'  expPageNum=%d\n",
	    seg->colID, seg->expPageNum);
        fprintf (stderr, "          : seqno: %d dca_tid; %d\n", 
	    seg->seqno, dca_tid);
    }

    nx 		= 2112;			/* NEWFIRM SPECIFIC		*/
    ny 		= 2048;			/* NEWFIRM SPECIFIC		*/
    ip 		= idata;                /* initialize                 	*/
    datalen 	= 2112 * 4 * 32;	/* 32 rows of int => 64 chunks  */
    swapped 	= 1;
    nstreams 	= 1;

    st.offset 	= 0;
    st.npix 	= datalen / 4;
    st.stride 	= 1;
    st.xydir 	= 0;
    st.x0 	= 0;
    st.y0 	= 0;

#ifdef KPNO

#ifdef FALL07_LAYOUT
    /*  Set up the detector quadrants to go into the image extensions using
    **  the usual convention of numbering from the LLC.  The output image
    **  will contain:
    **
    **		+---------------------------+
    **		| im3=PanB-2  |  im4=PanA-2 |
    **		+---------------------------+
    **		| im1=PanB-1  |  im2=PanA-1 |
    **		+---------------------------+
    */
    if (     strncmp (seg->colID, "PanA", 4) ==0 && seg->expPageNum == 1)
        st.destimage = 2;
    else if (strncmp (seg->colID, "PanA", 4) ==0 && seg->expPageNum == 2)
        st.destimage = 4;
    else if (strncmp (seg->colID, "PanB", 4) ==0 && seg->expPageNum == 1)
        st.destimage = 1;
    else if (strncmp (seg->colID, "PanB", 4) ==0 && seg->expPageNum == 2)
        st.destimage = 3;

#else
    /*  Set up the detector quadrants to go into the image extensions using
    **  the usual convention of numbering from the LLC.  The output image
    **  will contain:
    **
    **		+---------------------------+
    **		| im3=PanB-2  |  im4=PanB-1 |
    **		+---------------------------+
    **		| im1=PanA-2  |  im2=PanA-1 |
    **		+---------------------------+
    */
    if (     strncmp (seg->colID, "PanA", 4) ==0 && seg->expPageNum == 1)
        st.destimage = 2;
    else if (strncmp (seg->colID, "PanA", 4) ==0 && seg->expPageNum == 2)
        st.destimage = 4;
    else if (strncmp (seg->colID, "PanB", 4) ==0 && seg->expPageNum == 1)
        st.destimage = 3;
    else if (strncmp (seg->colID, "PanB", 4) ==0 && seg->expPageNum == 2)
        st.destimage = 1;

fprintf (stderr, "\ncolID = '%s'  pageNum = %d  ==> destimage = %d\n\n",
    seg->colID, seg->expPageNum, st.destimage);
#endif

#else
#ifdef CTIO

    /*  Set up the detector quadrants to go into the image extensions using
    **  the usual convention of numbering from the LLC.  The output image
    **  will contain:
    **
    **		+---------------------------+
    **		| im3=PanA-1  |  im4=PanB-2 |
    **		+---------------------------+
    **		| im1=PanA-2  |  im2=PanB-1 |
    **		+---------------------------+
    */
    if (     strncmp (seg->colID, "PanA", 4) ==0 && seg->expPageNum == 1)
        st.destimage = 3;
    else if (strncmp (seg->colID, "PanA", 4) ==0 && seg->expPageNum == 2)
        st.destimage = 1;
    else if (strncmp (seg->colID, "PanB", 4) ==0 && seg->expPageNum == 1)
        st.destimage = 2;
    else if (strncmp (seg->colID, "PanB", 4) ==0 && seg->expPageNum == 2)
        st.destimage = 4;

fprintf (stderr, "\ncolID = '%s'  pageNum = %d  ==> destimage = %d\n\n",
    seg->colID, seg->expPageNum, st.destimage);
#endif

#endif



    for (j=0; j < nchunks; j++) {

        /* Initialize the WriteHeaderData request for the keyword group.
        */
        seqno = seg->seqno;
        pvm_initsend (PvmDataDefault);
        pvm_pkint (&seqno, 1, 1);
        pvm_pkint (&datalen, 1, 1);
        pvm_pkint (&swapped, 1, 1);
        pvm_pkint (&nstreams, 1, 1);

        st.y0 = (j * 32);	
	st.xstep = st.ystep = 1;

	pvm_pkint ((int *)&st, sizeof(st)/sizeof(int), 1);
	pvm_pkbyte ((char *)ip, datalen, 1);
	pvm_send (dca_tid, DCA_WritePixelData);

	if (console && verbose)
	    fprintf (stderr, "%d: (x,y) = (%d,%d) len=%d\n", j,
		st.x0, st.y0, datalen);

	if ((j % 16) == 0) {
	    pvm_initsend (PvmDataDefault);
	    pvm_pkint (&seqno, 1, 1);
	    pvm_send (dca_tid, DCA_Synchronize);

	    mb_timeout.tv_sec = MB_TIMEOUT;
	    mb_timeout.tv_usec = 0;
            if ((s=pvm_trecv (dca_tid, DCA_Synchronize, &mb_timeout)) <= 0) {
		if (console)
                    fprintf (stderr, "pxf: %s", (s == 0) ? 
                        "timeout waiting for response from DCA\n" :
                        "failed to synchronize with DCA");
            }
	}
	ip += st.npix;
	nbytes += datalen;
    }

    /* Send final Sync.
    */
    pvm_initsend (PvmDataDefault);
    pvm_pkint (&seqno, 1, 1);
    pvm_send (dca_tid, DCA_Synchronize);
    mb_timeout.tv_sec = MB_TIMEOUT;
    mb_timeout.tv_usec = 0;
    if ((s=pvm_trecv (dca_tid, DCA_Synchronize, &mb_timeout)) <= 0) {
	if (console) {
            fprintf (stderr, "pxf: %s", (s == 0) ? 
                 "timeout waiting for response from DCA\n" :
                 "failed to synchronize with DCA");
        }
    }

    if (console) {
	fprintf (stderr, ": nchunks=%d  %d bytes  %d pixels\n",
		nchunks, nbytes, nbytes / 4);
    }

#ifdef DO_TIMINGS
    t2 = time ((time_t)NULL);
    fprintf (stderr, "Time [%d] procDCAData()\n", (t2-t1));
#endif
}


/*  ENCODE_I4 -- Encode a 4 byte integer as a byte sequence.
encode_i4 (val)
int val;
{
    int oval = 0;
    register unsigned char *op = (unsigned char *) &oval;

    *op++ = ((val      ) & 0377);
    *op++ = ((val >>  8) & 0377);
    *op++ = ((val >> 16) & 0377);
    *op++ = ((val >> 24) & 0377);

    return (oval);
}
*/


/*  INKEYWLIST -- See whether the given keyword and database are in the 
**  monitor list.  The list of keywords isn't expected to be long so we
**  skip any optimizations.
*/
int
inKeywList (char *kwdb, char *keyw)
{
    int  i;
    char  kw[SZ_LINE], *ip, *k;


    for (i=0; i < NKeywords; i++) {
	if (strncasecmp (keywList[i], "any", 3) == 0) {
	    /* If the keyword can come from any database, match only on
	    ** the keyword itself.
	    */
            k = keywList[i];
            if (strcasecmp (&k[4], keyw) == 0)
	        return (1);

	} else {
	    /* Otherwise, we were given the keyword list with a specific
	    ** database name in the form "db.keyw" so construct a string
	    ** for the match.
	    */
	    sprintf (kw, "%s.%s\0", kwdb, keyw);
            if (strcasecmp (keywList[i], kw) == 0)
	        return (1);
	}
    }

    return (0);
}
