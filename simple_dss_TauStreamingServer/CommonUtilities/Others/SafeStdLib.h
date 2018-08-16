/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 SafeStdLib.h
Description: Some defs for Thread safe std lib calls.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/


#ifndef _INTERNAL_STDLIB_H_
#define _INTERNAL_STDLIB_H_

#include <time.h>

#define kTimeStrSize 32
#define kErrorStrSize 256
	extern int qtss_maxprintf(const char *fmt, ...);
    extern void qtss_setmaxprintfcharsinK(unsigned long newMaxCharsInK);
    extern unsigned long qtss_getmaxprintfcharsinK();

#ifndef USE_DEFAULT_STD_LIB

#include <stdio.h>
#include <stdlib.h>



#ifdef __cplusplus
extern "C" {
#endif

#ifdef __USE_MAX_PRINTF__
	#define qtss_printf qtss_maxprintf
#else
    extern int qtss_printf(const char *fmt, ...);
    
#endif

extern int qtss_sprintf(char *buffer, const char *fmt,...);
extern int qtss_fprintf(FILE *file, const char *fmt, ...);
extern int qtss_snprintf(char *str, size_t size, const char *format, ...);
extern size_t qtss_strftime(char *buf, size_t maxsize, const char *format, const struct tm *timeptr);

// These calls return the pointer passed into the call as the result.

extern char *qtss_strerror(int errnum, char* buffer, int buffLen);
extern char *qtss_ctime(const time_t *timep, char* buffer, int buffLen);
extern char *qtss_asctime(const struct tm *timeptr, char* buffer, int buffLen);
extern struct tm *qtss_gmtime (const time_t *, struct tm *result);
extern struct tm *qtss_localtime (const time_t *, struct tm *result);

#ifdef __cplusplus
}
#endif


#else //USE_DEFAULT_STD_LIB

#define qtss_sprintf sprintf
#define qtss_fprintf fprintf

#ifdef __USE_MAX_PRINTF__
	#define qtss_printf qtss_maxprintf
#else
	#define qtss_printf printf
#endif

#define qtss_snprintf snprintf
#define qtss_strftime strftime

// Use our calls for the following.
// These calls return the pointer passed into the call as the result.

    extern char *qtss_strerror(int errnum, char* buffer, int buffLen);
    extern char *qtss_ctime(const time_t *timep, char* buffer, int buffLen);
    extern char *qtss_asctime(const struct tm *timeptr, char* buffer, int buffLen);
    extern struct tm *qtss_gmtime (const time_t *, struct tm *result);
    extern struct tm *qtss_localtime (const time_t *, struct tm *result);

#endif  //USE_DEFAULT_STD_LIB

#endif //_INTERNAL_STDLIB_H_


