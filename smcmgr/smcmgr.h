/* SMCMGR.H -- Include file definitions for the SMC Manager.
*/


/* Mac OSX doesn't provide the same shmem definitions as linux.
*/

#ifdef MACOSX
/* ipcs ctl commands
*/
# define SHM_STAT       13
# define SHM_INFO       14

/* shm_mode upper byte flags
*/
#define SHM_DEST       01000   /* segment will be destroyed on last detach  */
#define SHM_LOCKED     02000   /* segment will not be swapped 		    */
#define SHM_HUGETLB    04000   /* segment is mapped via hugetlb 	    */

struct  shminfo {
    unsigned long int shmmax;
    unsigned long int shmmin;
    unsigned long int shmmni;
    unsigned long int shmseg;
    unsigned long int shmall;
    unsigned long int __unused1;
    unsigned long int __unused2;
    unsigned long int __unused3;
    unsigned long int __unused4;
};

struct shm_info {
    int used_ids;
    unsigned long int shm_tot;  	/* total allocated shm 		    */
    unsigned long int shm_rss;  	/* total resident shm 		    */
    unsigned long int shm_swp;  	/* total swapped shm 		    */
    unsigned long int swap_attempts;
    unsigned long int swap_successes;
};
#endif /* MACOSX */


/* Sky-subtraction buffers.
*/

#define  MAX_SKY_FRAMES		4	/* NEWFIRM specific		    */

typedef struct {
    char    colID[SZ_PATH];   		/* collector ID name                */
    int     expPageNum;       		/* page num. w/in this exposure     */
    int     dsize;			/* size of data array (bytes) 	    */
    int    *data;			/* ptr to pixel data 		    */
} skyBuf;


typedef struct {
    double  expID;			/* skyframe exposure ID 	    */
    char    obsSetID[SZ_LINE];          /* skyframe observation ID          */
    skyBuf  frame[MAX_SKY_FRAMES];	/* detector readouts		    */
} skyFrame, *skyFramePtr;
