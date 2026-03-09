#ifndef _PID_HDR_
#define _PID_HDR_


/*
 * a Pid is used to distinguish between the different fake applications
 * it is implemented as an unsigned long, but is guaranteed not to exceed
 * MAX_PID
 */

typedef unsigned long Pid;
#define MAX_PID 4

#endif /* _PID_HDR_ */
