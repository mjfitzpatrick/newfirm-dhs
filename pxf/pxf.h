/******************************************************************************
 * define(s)
 ******************************************************************************/
/******************************************************************************
 * typedef(s)
 ******************************************************************************/

/*******************************************************************************
 * prototype(s)
 ******************************************************************************/

extern char pxfDIR[256];
extern char pxfFILENAME[256];
extern int pxfFLAG;
extern int pxfFileCreated;
extern int pxfExtChar;
extern int procDebug;


/* Keyword monitoring structs.
*/

#define	SZ_KEYWLIST		100

char	*keywList[SZ_KEYWLIST];		/* list of keywords to monitor	*/
int	NKeywords;			/* No. of keywords monitored	*/
