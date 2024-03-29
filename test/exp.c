#include "mbus.h"


/*  Client program to generate a fake exposure to the NDHS simulator.
**  All we really do here is connect to the message bus and issue a
**  START command to the nocs/pan simulators.
*/


int loop 	= 1;
int verbose 	= 1;
int delay 	= 0;
int interactive = 0;

extern char *pTime(char *buf);


int 
main(int argc, char *argv[])
{
    int    i, mytid, expnum = 0;
    char  *group = "trigger";
    char   buf[SZ_FNAME], msg[SZ_FNAME];
    double expID = 2450000.0;


    /* Initialize connections to the message bus.
    */
    if ((mytid = mbusConnect("CMD_Test", group, FALSE)) <= 0) {
	fprintf(stderr, "ERROR: Can't connect to message bus.\n");
	exit(1);
    }



    /* Process the command-line arguments.
     */
    for (i = 1; i < argc; i++) {
	if (strncmp(argv[i], "-help", 2) == 0) {
	    fprintf (stderr,
		"Usage: exp [-loop <N>] [-delay <nsec>] [-interactive]\n");
	    exit(0);;

	} else if (strncmp(argv[i], "-group", 2) == 0) {
	    group = argv[++i];

	} else if (strncmp(argv[i], "-expid", 2) == 0) {
	    i++;		/* no-op */

	} else if (strncmp(argv[i], "-start", 2) == 0) {
	    sprintf (msg, "%.4f\0", (expID += 0.1));
	    mbusBcast(group, msg, MB_START);

	} else if (strncmp(argv[i], "-delay", 2) == 0) {
	    delay = atoi(argv[++i]);

	} else if (strncmp(argv[i], "-interactive", 2) == 0) {
	    interactive++;
	    loop += 999;

	} else if (strncmp(argv[i], "-loop", 2) == 0) {
	    loop = atoi(argv[++i]);

	} else if (strncmp(argv[i], "-verbose", 2) == 0) {
	    verbose++;
	
	} else {
	    fprintf (stderr, "ERROR: Unknown flag '%s'\n", argv[i]);
	}
    }


    if (interactive) {

        printf ("\n\n\n");
        while (1) {
            printf ("Hit <cr> to begin exposure, 'q' to quit....");
            if (fgets (buf, 80, stdin) == NULL || buf[0] == '\n') {

		sprintf (msg, "%.4f\0", (expID += 0.1));
		printf ("Starting exposure #%d:  ID:%.2lf  ", 
		    ++expnum, expID);

	        sendExposure ("NOCS_Test", expID);

            } else if (buf[0] == 'q')
                break;

            else
                fprintf (stderr, "Unknown command '%s'\n", buf);
	}

    } else {
        for (i=1; i <= loop; i++) {
	    fprintf (stderr, "%s Exp #%d  ID:%.2lf ",
		pTime(buf), i, (expID += 0.1));

	    sendExposure ("NOCS_Test", expID);

	    if (delay > 0)			/* wait for the next one  */
	        sleep (delay);	
        }
    }



    /* Disconnect from the message bus.
    */
    if (interactive) {
        printf ("Hit <cr> to quit ....\n");
        if (fgets (buf, 80, stdin) == NULL || buf[0] == '\n')
	    ;
    }

    mbusDisconnect (mytid);
    exit(0);
}



sendExposure (char *group, double expID)
{
    int from_tid = -1, to_tid = -1, subject = -1;
    char *host, msg[SZ_LINE];


    bzero (msg, SZ_LINE);
    sprintf (msg, "%.4lf\0", expID);
    mbusBcast (group, msg, MB_START); 	/* start the exposure     */

    printf(".... waiting for completion ... ");

    while (mbusRecv(&from_tid, &to_tid, &subject, &host, &msg) >= 0) {
	if (subject == MB_FINISH)
	    break;
    }
    printf("done.\n");

    if (host) free ((void *) host);
}

