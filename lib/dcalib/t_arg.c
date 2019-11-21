#if !defined(_dhsUtil_H_)
#include "dhsUtil.h"
#endif
#if !defined(_dhsImpl_H_)
#include "dhsImplementationSpecifics.h"
#endif


#include <stdlib.h>
#include <stdarg.h>
#include "dcaDhs.h"
main()
{
   int stat;
stat = dcaSendMsg (socket, dcaFmtMsg (DCA_CLOSE, (int )NULL, "string arggg"));

}
msgHdrPtr
dcaFmtMsg (ulong vtype, ulong whoami, ...)

{
    va_list ap;  /* pints to each unnamed arg in turn */

    char *sval;

    /* Struct is freed when we send the message. */
    msgHdrPtr mh = calloc (1, sizeof (msgHeader));

    mh->type   = vtype;
    mh->whoami = whoami;

    /* from global dhs state struct */

    mh->expnum = dhs.expID;	     /* from global dhs state struct */
    strncpy(mh->obset, dhs.obsSetID, 80); 

    va_start(ap, whoami);
    mh->addr   = (long)va_arg(ap, void *);
    if (mh->addr == (long) NULL) {
	va_end(ap);
	return mh;
    }
    mh->size  = va_arg(ap, int);

    /* Get 3rd argument */
    sval = va_arg(ap, char *); 
    printf("3rdarg: %s\n",sval);
    va_end(ap);

    return mh;
}



/* dcaSendMsg ( dhsChan *chan, msgHdrPtr msg) */
int dcaSendMsg (int socket, msgHdrPtr msg)
{

    int size;

    size = sizeof(msgType);
    return(dcaSend (socket, (char *)msg, size));

}


int dcaRecvMsg (int socket, char *client_data, int size)
{
    int istat;

    istat = dcaRecv (socket, client_data, size);
    if (istat != size) {
	return (DCA_ERR);
    }
    return istat;
}


/* dcaRefreshState -- Tell the supervisor to send us all the state
 * information we could possbly want.
dcaRefreshState (int chan)
{
}
 */

int dcaSendfpConfig (int socket, ulong whoAmI, char *buf, int size)
{
    msgHdrPtr msg;

    msg=dcaFmtMsg (DCA_SET|DCA_FP_CONFIG, whoAmI);
    if (dcaSendMsg (socket, msg) == DCA_ERR)
        return DCA_ERR;

    /* Now send the data */
    return(dcaSend (socket, buf, size));
}
