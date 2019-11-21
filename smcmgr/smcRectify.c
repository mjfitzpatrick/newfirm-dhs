#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>

#include "cdl.h"
#include "mbus.h"
#include "smCache.h"


/*  SMCRECTIFY -- Rectify the display area of the DATA pages being processed
**  depending on their location in the focal plane.  Write the result back
**  to the SMC page in the proper orientation.
*/


extern 	int trim_ref;			/* trim reference pix flag 	*/
extern  int rotate_enable;
extern  int console, verbose, debug;


#define REFERENCE_WIDTH		64	/* for NEWFIRM			*/
#define BITPIX			32

/*
#define	CTIO		1
*/
#define	KPNO		1


void smcRectify (smCache_t *smc, double expID);
void smcRectifyPage (smcPage_t *page);


    
/*  smcRectify -- Rectify all the rasters of an exposure given an expID.
*/
void
smcRectify (smc, expID)
smCache_t *smc;
double expID;
{
    smcPage_t *page;

    while ((page = smcNextByExpID (smc, expID))) {
        smcAttach (smc, page);
	if (page->type == TY_DATA || page->type == TY_VOID)
	    smcRectifyPage (page);
        smcDetach (smc, page, FALSE);
    }
}


/*  smcRectify -- Rectify the display pixels of an SMC page.
*/
void
smcRectifyPage (page)
smcPage_t *page;
{
    int    i, nx=0, ny=0, xs=0, ys=0, bitpix=BITPIX;
    int    type=4, dir=0, rowlen, nb, nr;
    XLONG  *adrop, *pix, *dpix, *tpix, *ip, *op;
    static XLONG *dpix_buf=(XLONG *)NULL, sv_nx=0, sv_ny=0, first_time=1;

    fpConfig_t *fp;


    if (!rotate_enable)			/* sanity checks		*/
	return;

    fp = smcGetFPConfig (page);		/* get the focal plane config	*/
    nx = fp->xSize;
    ny = fp->ySize;
    xs = fp->xStart;
    ys = fp->yStart;

    /* Get the pixel pointer and raster dimensions.  Allocate space that 
    ** includes the reference in case we use it later.
    */
    pix  = (XLONG *) smcGetPageData (page);

    if ((nx*ny) != (sv_nx*sv_ny)) {
        if (dpix_buf) free ((XLONG *)dpix_buf);
        sv_nx = nx;
        sv_ny = ny;
        first_time = 1;
    }
    if (first_time) {
        dpix = dpix_buf = (XLONG *) calloc ((nx*ny), sizeof (XLONG));
        first_time = 0;
    } else
        dpix = (XLONG *)dpix_buf;


    /* Figure out which raster this is.
    */
#ifdef  KPNO

#ifdef DISPLAY_ORIGINAL_DETECTOR
    if (xs == 0 && ys == 0) 	 dir = 0;		/* A1/Pa	*/
    else if (xs != 0 && ys == 0) dir = 1;		/* A2/Pa	*/
    else if (xs == 0 && ys != 0) dir = 2;		/* A2/Pb	*/
    else if (xs != 0 && ys != 0) dir = 4;		/* A1/Pb	*/
#else

#ifdef DISPLAY_FALL07_LAYOUT
    if (xs == 0 && ys == 0) 	 dir = 1;		/* A1/Pa	*/
    else if (xs != 0 && ys == 0) dir = 4;		/* A2/Pa	*/
    else if (xs == 0 && ys != 0) dir = 3;		/* A2/Pb	*/
    else if (xs != 0 && ys != 0) dir = 2;		/* A1/Pb	*/

#else

#ifdef DISPLAY_FEB08_ORIGINAL
    if (xs == 0 && ys == 0) 	 dir = 1;		/* A1/Pa	*/
    else if (xs != 0 && ys == 0) dir = 3;		/* A2/Pa	*/
    else if (xs == 0 && ys != 0) dir = 4;		/* A2/Pb	*/
    else if (xs != 0 && ys != 0) dir = 2;		/* A1/Pb	*/

#else
    if (xs == 0 && ys == 0) 	 dir = -3;		/* A1/Pa	*/
    else if (xs != 0 && ys == 0) dir = 0;		/* A2/Pa	*/
    else if (xs == 0 && ys != 0) dir = -2;		/* A2/Pb	*/
    else if (xs != 0 && ys != 0) dir = -4;		/* A1/Pb	*/
#endif
#endif
#endif

#else

#ifdef CTIO
         if (xs == 0 && ys == 0) dir = -4;		/* A1/Pa	*/
    else if (xs != 0 && ys != 0) dir = -3;		/* A1/Pb	*/
    else if (xs != 0 && ys == 0) dir = -2;		/* A2/Pa	*/
    else if (xs == 0 && ys != 0) dir =  0;		/* A2/Pb	*/
#endif

#endif


    if (dir == 0  || rotate_enable == 0)
	return;					/* nothing to do	*/


    ip = pix;				/* extract display area	to dpix */
    op = dpix;
    rowlen = nx - REFERENCE_WIDTH;
    nb = rowlen * (bitpix / 8);
    for (i=0; i < ny; i++) {
        bcopy (ip, op, nb);
	ip += rowlen + REFERENCE_WIDTH;
	op += rowlen;
    }

    /* Transform the raster.
    */
    nr = (rowlen*ny) * (bitpix/8);
    if (dir > 0) { 
        rotate ((void *)dpix, &adrop, type, rowlen, ny, dir);
        bcopy ((XLONG *)adrop, (XLONG *)dpix, nr);

    } else if (dir < 0) { 
	/* Add an extra X-flip to the rotation. */
        rotate ((void *)dpix, &adrop, type, rowlen, ny, -dir);
        bcopy ((XLONG *)adrop, (XLONG *)dpix, nr);
        rotate ((void *)dpix, &adrop, type, rowlen, ny, 1);
        bcopy ((XLONG *)adrop, (XLONG *)dpix, nr);
    }

    /* Now write the rectified pixels back to the SMC data page, taking
    ** into account the reference strip which we want to keep on the right
    ** side of the array.  NOTE:  The reference strip is related to the
    ** readout order of the detector, keeping in on the right side of the
    ** data array is strictly a convenience for us, after transformation the
    ** reference rows are not really associated with the rows in which they
    ** appear in the final raster.
    */
    ip = (XLONG *) adrop;
    op = pix;
    for (i=0; i < ny; i++) {
        bcopy (ip, op, nb);
	ip += rowlen;
	op += rowlen + REFERENCE_WIDTH;
    }

    /*free ((XLONG *)dpix); 				/* clean up.	*/
}
