/*
**  MBUS.H -- Global definitions for the message bus interface.
*/


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>


/*
#define MB_DEBUG	0
*/
#define MB_DEBUG     (getenv("MB_DEBUG")!=NULL||access("/tmp/MB_DEBUG",F_OK)==0)
#define MB_VERBOSE   ((getenv("MB_VERBOSE")!=NULL))


#define	SZ_LINE		1024
#define	SZ_FNAME	64
#define SZ_PATH		164


/*  Recognized DHS Client types.
 */ 
#define SUPERVISOR      	"super"
#define COLLECTOR       	"collector"
#define RTD             	"rtd"
#define PICFEED         	"picfeed"
#define DCA             	"dca"
#define ANY             	"any"
#define CLIENT             	"client"	/* Generic Client app	*/

#define	USE_ACK			0
 

/* Application state get/set params.
**/
#define	APP_TID			0	/* app's tid			*/
#define	APP_STID		1	/* supervisor's tid		*/
#define	APP_NAME		2	/* app's name			*/
#define	APP_INIT		3	/* is app initialized?		*/
#define	APP_FD			4	/* fd of msgbus			*/
#define	APP_MBUS		5	/* mbus backpointer		*/


 
/* MBUS Process states. */
#define MB_ACK                 0000001
/*
#define MB_IDLE                0000001
*/
#define MB_READY               0000002
#define MB_START               0000004
#define MB_SET                 0000010
#define MB_GET                 0200020
/*
#define MB_READING             0000010
#define MB_WRITING             0000020
*/
#define MB_FINISH              0000040
#define MB_DONE                0000100
#define MB_STATUS              0000200
#define MB_PING                0000400
 
#define MB_ERR                 0001000
#define MB_CONNECT             0002000
#define MB_DISCONNECT          0004000
#define MB_EXITING             0010000
#define MB_ABORT               0020000
#define MB_ORPHAN              0040000


 

#define MB_CONNECT_RETRYS	3	/* Number of connection attempts    */


#ifdef OK
#undef OK
#endif
#define OK      0
 
#ifdef ERR
#undef ERR
#endif
#define ERR     -1
 
#ifdef TRUE
#undef TRUE
#endif
#define TRUE   	1
 
#ifdef FALSE
#undef FALSE
#endif
#define FALSE	0
 
 
 
/* MBUS Locations -- We define only the extreme case of broadcasting to
** all clients of the particular type, all other times the 'where' field
** of a message is a specific node name.
**/
#define MB_ANY                  -1
#define MB_ALL                  -1
#define MB_NONE                  0
 


/* MBUS Data Structures --
**
*/

typedef int (*mbFunc)();		/* */
typedef void (*mbMethod)();

#define MAX_HANDLERS			16

typedef struct {
    int     fd;				/* handler file descriptor 	    */
    mbFunc  func;			/* handler function 		    */
    void   *client_data;		/* client data ptr 		    */
} mbHandler, *mbHandlerPtr;



#ifdef _LIBMBUS_SOURCE_

typedef struct {
    char hostfile[SZ_PATH];		/* hosts configuration file	    */
    int  initialized;			/* message bus initialized?	    */

    int  new_pvm;			/* new PVM started?  		    */
    int  groups_alive;			/* DHS Group started?  		    */

    int       nhandlers;		/* number of input handlers	    */
    mbHandler handlers[MAX_HANDLERS];	/* input handlers 		    */

} MBus, *MBusPtr;

typedef struct {
    char actual[SZ_FNAME];		/* Actual host name		    */
    char simulated[SZ_FNAME];		/* Simulated host name		    */
    char domain[SZ_FNAME];		/* Domain name		    	    */
    char ip_addr[SZ_FNAME];		/* IP addr of host we're using	    */
    int  use_sim;			/* Use simulated name?		    */
} MBHost, *MBHostPtr;
#endif



/* MBUS Function Prototypes --
**
*/

#ifdef _LIBMBUS_SOURCE_
					/* Public procedures		    */
int	mbusAddInputHandler (int fd, mbFunc handler, void *context);
int	mbusRemoveInputHandler (int fd);

void	mbusExitHandler ();

void	mbErrorExit ();
void	mbQuit ();

void	mbInitApp ();			/* Application State interface 	    */
int 	mbAppGet (int what);
char   *mbAppGetName ();
MBusPtr mbAppGetMBus ();
void 	mbAppSet (int what, int val);
void 	mbAppSetName (char *name);
void 	mbAppSetMBus (MBusPtr mbus);

fd_set	mbInputSet ();

#endif



/* MBUS Utility Macros
 */
#define isSupervisor(name)     (strncmp(SUPERVISOR,name,strlen(name))==0)

