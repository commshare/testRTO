
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 daemon.h
Description: Provide the explicit code definition of getopt function.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#ifndef GETOPT_H
#define GETOPT_H

#ifdef __cplusplus
extern "C" {
#endif


extern char *optarg;
extern int optreset;
extern int optind;
extern int opterr;
extern int optopt;
int getopt(int argc, char* const *argv, const char *optstr);


#ifdef __cplusplus
}
#endif

#endif /* GETOPT_H */
