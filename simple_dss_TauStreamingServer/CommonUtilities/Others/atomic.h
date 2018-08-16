/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 daemon.h
Description: Provide some utilities to implement the atomic operations.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#ifndef _ATOMIC_H_
#define _ATOMIC_H_

#ifdef __cplusplus
extern "C" {
#endif


extern unsigned int compare_and_store(unsigned int oval,unsigned int nval, unsigned int *area);

extern unsigned int atomic_add(unsigned int *area, int val);

extern unsigned int atomic_or(unsigned int *area, unsigned int mask);

extern unsigned int atomic_sub(unsigned int *area, int val);


#ifdef __cplusplus
}
#endif

#endif /* _ATOMIC_H_ */
