
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 md5.h
Description: Implemention to the MD5 Message-Digest Algorithm.
Comment:     copy from Darwin Streaming Server 5.5.5 and MD5.C - RSA Data Security
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 


#ifndef _MD5_H_
#define _MD5_H_

#include "OSHeaders.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MD5 context. */
typedef struct {
  UInt32 state[4];                                   /* state (ABCD) */
  UInt32 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;

void MD5_Init(MD5_CTX *context);
void MD5_Update(MD5_CTX *context, unsigned char *input, unsigned int inputLen);
void MD5_Final(unsigned char digest[16], MD5_CTX *context);

#ifdef __cplusplus
}
#endif

#endif //_MD5_H_

 




