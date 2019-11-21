/*
 * DSO.H -- Public definitions for the DSO interfaces.
 */

/* Types. */
typedef void *pointer;

#ifndef OK
#define OK 0
#endif
#ifndef ERR
#define ERR (-1)
#endif
#ifndef EOS
#define EOS '\0'
#endif

/* Access modes. */
#define DSO_RDONLY		0
#define DSO_RDWR		1
#define DSO_CREATE		2

/* Data types. */
#define	DSO_UBYTE		1
#define	DSO_CHAR		2
#define	DSO_SHORT		3
#define	DSO_USHORT		4
#define	DSO_INT			5
#define	DSO_LONG		6
#define	DSO_REAL		7
#define	DSO_DOUBLE		8

/* Generic message tags. */
#define MSG_Quit		1001
#define MSG_SetParam		1002
#define MSG_GetParam		1003
#define MSG_GetStatus		1004

/* Compatibility garbage. */
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
