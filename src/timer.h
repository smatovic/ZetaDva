/* clperft, a simple chess performance test written in opencl  */

#ifndef TIMER_H_INCLUDED
#define TIMER_H_INCLUDED

#include <string.h>     /* for string compare */ 
#include <time.h>       /* for time measurent */
#include <sys/time.h>   /* for gettimeofday */

double get_time(void);
void get_time_string(char *string);

#endif /* TIMER_H_INCLUDED */

