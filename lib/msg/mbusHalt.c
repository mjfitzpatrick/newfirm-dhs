/*  
*/

#include <stdio.h>
#include <sys/signal.h>
#include <pvm3.h>

#define _LIBMBUS_SOURCE_
#include "mbus.h"



/*  MBUSHALT -- 
 */
int
mbusHalt ()
{
    return (pvm_halt());
}
