/*******************************************************************************
 * include(s):
 ******************************************************************************/
#if !defined(_dhsUtil_H_)
#include "dhsUtil.h"
#endif
#if !defined(_FITSIO_H)
#include "fitsio.h"
#endif
#include <errno.h>

#include "pxf.h"
/*******************************************************************************
 * pxfSendMetaData ( ... )
 ******************************************************************************/
void pxfSendMetaData(XLONG *istat,	/* inherited status                */
		     char *resp,	/* response message                */
		     void *blkAddr,	/* address of data block           */
		     size_t blkSize,	/* size of data block              */
		     mdConfig_t * cfg,	/* configuration of meta data      */
		     double *expID,	/* exposure identifier             */
		     char *obsetID,	/* observation set identifier      */
		     fitsfile * fitsID)
{
    /* declare some variable and initialize them */
    int nr = 0;
    int fc = 0;
    int cpos = 0;
    char *op;
    int fitsStatus = 0;
    char *avpName[] = { "Name", "Value", "Comment" };
    char *avpFormat[] = { "A32", "A32", "A64" };
    char *avpUnits[] = { "string", "string", "string" };
    char avpCol1[DHS_AVP_NAMESIZE] = { '\0' };
    char avpCol2[DHS_AVP_VALSIZE] = { '\0' };
    char avpCol3[DHS_AVP_COMMENT] = { '\0' };
    char *avpRecord[4] = { avpCol1, avpCol2, avpCol3, (char *) NULL };
    char *fitsDir = (char *) NULL;
    char fitsFile[DHS_MAXSTR] = { '\0' };
    char fitsRecord[DHS_FITS_RECLEN] = { '\0' };
    struct stat fitsExists;

    /* checkSaver/ the inherited status */
    if (STATUS_BAD(*istat))
	return;
    MNSN_CLEAR_STR(resp, '\0');

    /* check input parameters */
    if (cfg == (mdConfig_t *) NULL || expID == (double *) NULL) {
	*istat = DHS_ERROR;
	sprintf(resp, "pxfSendMetaData: bad parameter, %s",
		((cfg ==
		  (mdConfig_t *) NULL)) ? "MetaData configuration==NULL" :
		"expID==NULL");
	return;
    }

    /* check address/size */
    if (blkAddr == (void *) NULL && blkSize != (size_t) 0) {
	*istat = DHS_ERROR;
	strcpy(resp,
	       "pxfSendMetaData: bad parameters BlkAddr==NULL and size!=0");
	return;
    }

    DPRINTF(15, procDebug, "pxfSendMetaData: blkSize = %ld\n", blkSize);
    /* return OK if size is 0 */
    if (blkSize == (size_t) 0) {
	*istat = DHS_OK;
	strcpy(resp, "pxfSendMetaData: Success. blkSize==0");
	return;
    }


    /* write headers here */
    switch (cfg->metaType) {
    case DHS_MDTYPE_FITSHEADER:	/* FITS header records */
	op = (char *) blkAddr;
	nr = (int) blkSize / DHS_FITS_RAWLEN;
	DPRINTF(200, procDebug, "pxfSendMetaData: NumRec= %ld\n", nr);
	for (fc = 0; fc < nr; fc++) {
	    DPRINTF(200, procDebug, "pxfSendMetaData: RecCount = %d\n",
		    fc);
	    DPRINTF(200, procDebug, "rec=>%-74s<\n", op);

	    /* initialize record */
	    cpos = 0;
	    (void) memset((void *) &fitsRecord[cpos], '\0',
			  (size_t) DHS_FITS_RECLEN);
	    /* copy name and increment pointers/counters */
	    (void) memmove((void *) &fitsRecord[cpos], (void *) op,
			   (size_t) DHS_FITS_NAMESIZE);
	    op += DHS_FITS_NAMESIZE;
	    cpos += DHS_FITS_NAMESIZE;
	    /* copy first separator and increment pointers/counters */
	    (void) memmove((void *) &fitsRecord[cpos], "= ",
			   (size_t) DHS_FITS_SEP1);
	    cpos += DHS_FITS_SEP1;
	    /* copy value and increment pointers/counters */
	    (void) memmove((void *) &fitsRecord[cpos], (void *) op,
			   (size_t) DHS_FITS_VALSIZE);
	    op += DHS_FITS_VALSIZE;
	    cpos += DHS_FITS_VALSIZE;
	    /* copy second separator and increment pointers/counters */
	    (void) memmove((void *) &fitsRecord[cpos], " / ",
			   (size_t) DHS_FITS_SEP2);
	    cpos += DHS_FITS_SEP2;
	    /* copy comment and increment pointers/counters */
	    (void) memmove((void *) &fitsRecord[cpos], (void *) op,
			   (size_t) DHS_FITS_COMMENT);
	    op += DHS_FITS_COMMENT;
	    DPRINTF(200, procDebug, "rec=>%-80s<\n", fitsRecord);
	    /* now write out the record */
	    if (ffprec(fitsID, fitsRecord, &fitsStatus)) {
		*istat = DHS_OK;
		sprintf(resp,
			"pxfSendMetaData: fits write failed at record %d with fitsStatus = %d",
			fc + 1, fitsStatus);
	    }
	}
	break;
    case DHS_MDTYPE_AVPAIR:	/* AVP header records as FITS ASCII table */
	nr = (int) blkSize / DHS_AVP_RAWLEN;
	/* create an ascii table */
	if (obsetID == (char *) NULL || strlen(obsetID) <= (size_t) 1) {
	    ffcrtb(fitsID, ASCII_TBL, nr, cfg->numFields,
		   (char **) &avpName, (char **) &avpFormat,
		   (char **) &avpUnits, "PAN AV-Pairs List", &fitsStatus);
	} else {
	    ffcrtb(fitsID, ASCII_TBL, nr, cfg->numFields,
		   (char **) &avpName, (char **) &avpFormat,
		   (char **) &avpUnits, obsetID, &fitsStatus);
	}
	if (fitsStatus) {
	    *istat = DHS_ERROR;
	    sprintf(resp,
		    "pxfSendMetaData:  fits ascii table create failed.  fitsStatus = %d",
		    fitsStatus);
	    break;
	}
	(void) ffpkyj(fitsID, "EXTTYPE", cfg->metaType, "meta data type",
		      &fitsStatus);
	/* populate the ascii table */
	op = (char *) blkAddr;
	for (fc = 0; fc < nr; fc++) {
	    (void) memmove((void *) &avpCol1[0], (void *) op,
			   (size_t) DHS_AVP_NAMESIZE);
	    op += DHS_AVP_NAMESIZE;
	    if (ffpcls
		(fitsID, 1, fc + 1, 1, 1, (char **) &avpRecord[0],
		 &fitsStatus)) {
		*istat = DHS_ERROR;
		sprintf(resp,
			"pxfSendMetaData:  fits ascii table write1 failed. fitsStatus = %d",
			fitsStatus);
		break;
	    }
	    (void) memmove((void *) &avpCol2[0], (void *) op,
			   (size_t) DHS_AVP_VALSIZE);
	    op += DHS_AVP_VALSIZE;
	    if (ffpcls
		(fitsID, 2, fc + 1, 1, 1, (char **) &avpRecord[1],
		 &fitsStatus)) {
		*istat = DHS_ERROR;
		sprintf(resp,
			"pxfSendMetaData:  fits ascii table write2 failed. fitsStatus = %d",
			fitsStatus);
		break;
	    }
	    (void) memmove((void *) &avpCol3[0], (void *) op,
			   (size_t) DHS_AVP_COMMENT);
	    op += DHS_AVP_COMMENT;
	    if (ffpcls
		(fitsID, 3, fc + 1, 1, 1, (char **) &avpRecord[2],
		 &fitsStatus)) {
		sprintf(resp,
			"pxfSendMetaData:  fits ascii table write3 failed. fitsStatus = %d",
			fitsStatus);
		break;
	    }
	}
	break;

    case DHS_MDTYPE_SHIFTLIST:	/* MONSOON Pixel shift list */
	*istat = DHS_OK;
	strcpy(resp,
	       "pxfSendMetaData:  metadata type SHIFTLIST not yet implemented");
	break;

    case DHS_MDTYPE_ARRAYDATA:	/* MONSOON data array ??? */
	*istat = DHS_OK;
	strcpy(resp,
	       "pxfSendMetaData:  metadata type ARRAYDATA not yet implemented");
	break;

    default:
	break;
    }

    /* flush and close */
    (void) ffflus(fitsID, &fitsStatus);
    fitsStatus = 0;
/*
    if ( fits_close_file(fitsID,&fitsStatus) ) 
    {
	*istat = DHS_ERROR;
	sprintf(resp,"pxfSendMetaData:  fits close failed. fitsStatus = %d", fitsStatus);
	return;
    }
*/
    /* return */
    strcpy(resp, "pxfSendMetaData:  Success");
    return;
}
