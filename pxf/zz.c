#include <string.h>
#include <stdio.h>

int main (int argc, char **argv)
{
    char *src = "12345           123456             1234567              ";
    char v1[20], v2[20], v3[20];
    int i;


    for (i=0; i < 1000; i++) {
        procDCAMoveStr (&v1, src, 16);
        procDCAMoveStr (&v2, &src[16], 16);
        procDCAMoveStr (&v3, &src[32], 16);

	if (1)
            printf ("v1='%s'  v2='%s'  v3 = '%s'\n", v1, v2, v3);
    }

    exit (0);
}


/*  Copy a string from an AVP buffer to a value, stripping leading and 
**  trailing whitespace in the process.  Both 'instr' and 'outstr' are 
**  assumed to be at least 'len' chars long.
*/

#define SZ_AVP_BUF	128

procDCAMoveStr (char **outstr, char *instr, int len)
{
    char *ip, buf[SZ_AVP_BUF];
    int i = len;

    
    bzero (buf, SZ_AVP_BUF);			/* zero buffer		*/
    bzero (outstr, len);			/* zero outstr		*/
    bcopy (instr, buf, len);			/* move the string 	*/

						/* skip trailing space	*/
    for (i=len-1; isprint(buf[i]) && isspace(buf[i]) && i; i--)
	buf[i] = '\0';				
						/* skip leading space	*/
    for (ip=&buf[0], i=0; isspace(*ip) && i < len; ip++)
	;					

    strcpy ((char *)outstr, ip);		/* move final string 	*/
}
