
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 md5digest.h
Description: Provides a function to calculate the md5 digest,given all the authentication parameters.
Comment:     copy from Darwin Streaming Server 5.5.5 and MD5.C - RSA Data Security
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 


#ifndef _MD5DIGEST_H_
#define _MD5DIGEST_H_

#include "StrPtrLen.h"

enum 
{
    kHashHexLen =   32,
    kHashLen    =   16
};

// HashToString allocates memory for hashStr->Ptr 
void HashToString(unsigned char aHash[kHashLen], StrPtrLen* hashStr);

// allocates memory for hashA1Hex16Bit->Ptr                   
void CalcMD5HA1(StrPtrLen* userName, StrPtrLen* realm, StrPtrLen* userPassword, StrPtrLen* hashA1Hex16Bit);

// allocates memory to hA1->Ptr
void CalcHA1( StrPtrLen* algorithm, 
              StrPtrLen* userName, 
              StrPtrLen* realm,
              StrPtrLen* userPassword, 
              StrPtrLen* nonce, 
              StrPtrLen* cNonce,
              StrPtrLen* hA1
            );

// allocates memory to hA1->Ptr
void CalcHA1Md5Sess(StrPtrLen* hashA1Hex16Bit, StrPtrLen* nonce, StrPtrLen* cNonce, StrPtrLen* hA1);

// allocates memory for requestDigest->Ptr               
void CalcRequestDigest( StrPtrLen* hA1, 
                        StrPtrLen* nonce, 
                        StrPtrLen* nonceCount, 
                        StrPtrLen* cNonce,
                        StrPtrLen* qop,
                        StrPtrLen* method, 
                        StrPtrLen* digestUri, 
                        StrPtrLen* hEntity, 
                        StrPtrLen* requestDigest
                      );


void to64(register char *s, register long v, register int n);

// Doesn't allocate any memory. The size of the result buffer should be nbytes
void MD5Encode( char *pw, char *salt, char *result, int nbytes);

#endif
