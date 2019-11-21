#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#include "dhsUtil.h"

main()
{
    long istat;
    char resp[80];
    long socket;
    char *blkAddr;
    size_t blkSize;
    mdConfig_t cfg;
    double expID;
    char *obsID;

    blkAddr = (char *) malloc(300 * 80);
    readHdr("preHeader.txt", blkAddr, &blkSize);
    printf("blkSize: %d\n", blkSize);
    dhsUtilSendMetaData(&istat, resp, socket, blkAddr, blkSize, &cfg,
			&expID, obsID);
}
void dhsUtilSendMetaData(long *istat,	/* inherited status  */
			 char *resp,	/* response message  */
			 long dhsID,	/* dhs handle        */
			 void *blkAddr,	/* address of data block */
			 size_t blkSize,	/* size of data block    */
			 mdConfig_t * cfg,	/* configuration of meta data */
			 double *expID,	/* exposure identifier        */
			 char *obsetID	/* observation set identifier */
    )
{
    char *line, *ch;
    int i, nch;

    line = (char *) blkAddr;
    for (i = 1; i < 30; i++) {
	if ((ch = strchr(line, '\n')) != NULL) {
	    ch++;
	}
	nch = ch - line;
	printf("%40.40s  nch:%d\n", ch, nch);
	line = line + nch;
    }

    printf("socket: %d, size: %d\n", dhsID, blkSize);
    return;
}

readHdr(char *file, char *blkAddr, size_t size)
{
    int fd, nchar;

    fd = open(file, O_RDONLY);
    size = read(fd, blkAddr, 300 * 80);
    printf("size: %d\n", size);
}
