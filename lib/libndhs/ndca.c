#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#include <pvm3.h>

#include "ndhs.h"


#define SZ_VALSTR       80
#define SZ_CMDSTR       512
#define MAX_KWGROUPS    64
#define MAX_CARDS       512
#define MAX_IGNORED     10
#define MAX_HDRFILES    16
#define MAX_DIRFILES    512
#define HDRFILE_WAIT    3               /* wait for hdrfile creation */
#define MAX_WAIT        15              /* max hdrfile wait, seconds */
#define DEF_PFDRETAIN   8               /* max old pictures to retain */
#define DEF_PFDSTALE    24              /* delete xpim files older than 24h
*/

#ifdef SZ_FNAME
#undef SZ_FNAME
#endif
#define SZ_FNAME        256

#ifdef SZ_PATHNAME
#undef SZ_PATHNAME
#endif
#define SZ_PATHNAME     512


extern int console, verbose;
extern int procDebug;
extern int noop;


/* Global data for conversion. */
static int seqno;
static int iostat;
static int picstat;
static int picnum;
static int nimages;
static int bitpix;
static int naxis;
static int nsync;

int     env_mbtimeout; 			/* Message bus timeout (seconds). */
int     mbtimeout = 30;			/* Message bus timeout (seconds). */




char imtype[SZ_VALSTR];
char imagetype[SZ_VALSTR];
char imagename[SZ_FNAME];

static struct timeval mb_timeout;
#define SET_TIMEOUT(tm,sec)  ((tm)->tv_sec = (sec), (tm)->tv_usec = 0)

extern int errno;
extern char *getenv();


static int GetMode (int dca_tid);
static int ndhsGetStatus (int dca_tid, int tag);



/****************************************************************************
** StartReadout -- 
*/
ndhsStartReadout (int dca_tid, int seqno, char *imname, char *imtype)
{
    pvm_initsend (PvmDataDefault);
    pvm_pkint (&seqno, 1, 1);
    pvm_pkstr (imname);
    pvm_pkstr (imtype);
    pvm_send (dca_tid, DCA_StartReadout);

    if (ndhsGetStatus (dca_tid, DCA_StartReadout) < 0) {
        dprintf (0, "ndhsStartReadout",
	    "DCA failed to create output image, seqno=%d\n", seqno);
        return (ERR);
    }
    return (OK);
}




/****************************************************************************
** EndReadout -- 
*/
ndhsEndReadout (int dca_tid, int seqno)
{
    pvm_initsend (PvmDataDefault);
    pvm_pkint (&seqno, 1, 1);
    pvm_send (dca_tid, DCA_EndReadout);


    if (ndhsGetStatus (dca_tid, DCA_EndReadout) < 0) {
        fprintf (stderr, "ndhsEndReadout: end readout failed, seqno=%d\n",
	    seqno);
	return (ERR);

    } else {
        fprintf (stderr, 
	    "ndhsEndReadout: readout finished with no errors, seqno=%d\n",
	    seqno);
        return (OK);
    }
}


/****************************************************************************
** TransferComplete -- 
*/
ndhsTransferComplete (int dca_tid, int seqno)
{
    pvm_initsend (PvmDataDefault);
    pvm_pkint (&seqno, 1, 1);
    pvm_send (dca_tid, DCA_TransferComplete);


/*
    if (ndhsGetStatus (dca_tid, DCA_TransferComplete) < 0) {
        fprintf (stderr, 
	    "ndhsTransferComplete: transfer complete failed, seqno=%d\n",seqno);
	return (ERR);

    } else {
        fprintf (stderr, 
	    "ndhsTransferComplete: finished with no errors, seqno=%d\n", seqno);
        return (OK);
    }
*/
}


/****************************************************************************
** SetDirectory -- Set the current working directory.
*/
ndhsSetDirectory (int dca_tid, int seqno, char *cwd)
{
    /* Send an advisory set directory request to the DCA (whether or not
     * the DCA actually allows a client to determine the output directory
     * is up to the DCA).
     */
    pvm_initsend (PvmDataDefault);
    pvm_pkint (&seqno, 1, 1);
    pvm_pkstr ("directory");
    pvm_pkstr (cwd);
    pvm_send (dca_tid, DCA_SetParam);

    return (OK);
}


/****************************************************************************
** SetKTMFile -- Set the current KTM file to execute.
*/
ndhsSetKTMFile (int dca_tid, int seqno, char *ktm)
{
    /* Set the param that specifies the KTM file to invoke.
     */
    pvm_initsend (PvmDataDefault);
    pvm_pkint (&seqno, 1, 1);
    pvm_pkstr ("ktm");
    pvm_pkstr (ktm);
    pvm_send (dca_tid, DCA_SetParam);

    return (OK);
}


/****************************************************************************
** SetExpID -- Set the current Exposure ID.
*/
ndhsSetExpID (int dca_tid, int seqno, double expID)
{
    char buf[64];

    bzero (buf, 64);
    sprintf (buf, "%lf\0", expID);

    pvm_initsend (PvmDataDefault);
    pvm_pkint (&seqno, 1, 1);
    pvm_pkstr ("expID");
    pvm_pkstr (buf);
    pvm_send (dca_tid, DCA_SetParam);

    return (OK);
}


/****************************************************************************
** SetObsID -- Set the current Observation Set ID.
*/
ndhsSetObsID (int dca_tid, int seqno, char *obsID)
{
    pvm_initsend (PvmDataDefault);
    pvm_pkint (&seqno, 1, 1);
    pvm_pkstr ("obsID");
    pvm_pkstr (obsID);
    pvm_send (dca_tid, DCA_SetParam);

    return (OK);
}


/****************************************************************************
** SetImIndex -- Set the current image index.
*/
ndhsSetImIndex (int dca_tid, int seqno, int index)
{
    char indx[32];

    sprintf (indx, "%d\0", index);

    pvm_initsend (PvmDataDefault);
    pvm_pkint (&seqno, 1, 1);
    pvm_pkstr ("imindex");
    pvm_pkstr (indx);
    pvm_send (dca_tid, DCA_SetParam);

    return (OK);
}





/****************************************************************************
** ConfigureImage -- Send the ConfigureImage request to the DCA to fix the
** imagefile geometry (file layout).  Once this is done we have a readable
** imagefile (but with no data) on the remote end, and we are ready to start
** sending pixels.
*/
ndhsConfigureImage (int dca_tid, int seqno, char *imname, char *imtype)
{
    int nx, ny, imageno, i, j;
    int extend = 0;

    int ndet    = 4;			/* ****  NEWFIRM Specific  ***** */
    int pixtype = DSO_INT;		/* ****  NEWFIRM Specific  ***** */
    int naxis   = 2;			/* ****  NEWFIRM Specific  ***** */
    int naxis1  = 2112;			/* ****  NEWFIRM Specific  ***** */
    int naxis2  = 2048;			/* ****  NEWFIRM Specific  ***** */


    fprintf (stderr, 
	"configure image:  seqno: %d name='%s' type='%s' ndet=%d...\n", 
	seqno, imname, imtype, ndet);

    pvm_initsend (PvmDataDefault);
    pvm_pkint (&seqno, 1, 1);
    pvm_pkstr (imname);
    pvm_pkstr (imtype);
    pvm_pkint (&ndet, 1, 1);

    for (i=j=0;  i < ndet;  i++) {
        nx = naxis1;
        ny = naxis2;

        pvm_pkint (&pixtype, 1, 1);
        pvm_pkint (&naxis, 1, 1);
        pvm_pkint (&nx, 1, 1);
        pvm_pkint (&ny, 1, 1);
        pvm_pkint (&extend, 1, 1);
        pvm_pkstr ("");
        pvm_pkstr ("");
    }


    pvm_send (dca_tid, DCA_ConfigureImage);
    if (ndhsGetStatus(dca_tid,DCA_ConfigureImage) < 0) {
        fprintf (stderr, "configure image fails, seqno=%d\n", seqno);
        picstat++;
        return (-1);
    } else
        fprintf (stderr, "configure image succeeds, seqno=%d\n", seqno);

    return (0);
}





/****************************************************************************
** GetMode -- Query the server for the current readout mode.
*/
static int
GetMode (int dca_tid)
{
    int s, mode = 0;

    pvm_initsend (PvmDataDefault);
    pvm_pkint (&seqno, 1, 1);
    if (pvm_send (dca_tid, DCA_GetStatus) < 0) {
        fprintf (stderr, "GetMode: pvm_send to DCA fails\n");
        picstat++;
        mode = -1;
    }

    SET_TIMEOUT (&mb_timeout, mbtimeout);
    if ((s = pvm_trecv (dca_tid, DCA_GetStatus, &mb_timeout)) <= 0) {
        fprintf (stderr, "GetMode (timeout=%d): %s", mb_timeout,
	    (s == 0) ?  "timeout waiting for response from DCA\n" :
    			"error reading from DCA\n");
        mode = -1;
        picstat++;
    } else {
        if (pvm_upkint (&mode, 1, 1) < 0)
    	mode = -1;
        if (pvm_upkint (&iostat, 1, 1) < 0)
    	mode = -1;
    }

    return (mode);
}


/****************************************************************************
** GetStatus -- Get a status packet.
*/
static int
ndhsGetStatus (int dca_tid, int tag)
{
    int s, status = 0;

    SET_TIMEOUT (&mb_timeout, mbtimeout);
    if ((s = pvm_trecv (dca_tid, tag, &mb_timeout)) <= 0) {
        fprintf (stderr, "GetStatus (timeout=%d, tag=%d): %s", mbtimeout, tag,
	    (s == 0) ?  "timeout waiting for response from DCA\n" :
    			"error reading from DCA\n");
        status = -1;
        picstat++;
    } else {
        if (pvm_upkint (&status, 1, 1) < 0)
    	status = -1;
        if (pvm_upkint (&iostat, 1, 1) < 0)
    	status = -1;
    }

    return (status);
}


/* HOST_SWAP2 -- Test whether the current host computer stores 2-byte short
 * integers with the bytes swapped (little-endian).
 */
host_swap2()
{
    static unsigned short val = 1;
    char *byte = (char *) &val;
    return (byte[0] != 0);
}


/* ENCODE_I4 -- Encode a 4 byte integer as a byte sequence.
 */
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


/* DEODE_I4 -- Decode a 4 byte integer stored as a byte sequence.
 */
decode_i4 (val)
int val;
{
    register unsigned char *ip = (unsigned char *) &val;
    int oval = 0;

    oval |= ((*ip++)      );
    oval |= ((*ip++) <<  8);
    oval |= ((*ip++) << 16);
    oval |= ((*ip++) << 24);

    return (oval);
}
