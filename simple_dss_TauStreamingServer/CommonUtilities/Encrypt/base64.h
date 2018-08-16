/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 base64.h
Description: Implemention to the base64 encode/decode algorithm.
Comment:     copy from Darwin Streaming Server 5.5.5 and Originally Apache file ap_base64.c
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 


#ifndef _BASE64_H_
#define _BASE64_H_

#ifdef __cplusplus
extern "C" {
#endif

int Base64encode_len(int len);
int Base64encode(char * coded_dst, const char *plain_src,int len_plain_src);

int Base64decode_len(const char * coded_src);
int Base64decode(char * plain_dst, const char *coded_src);

#ifdef __cplusplus
}
#endif

#endif //_BASE64_H_
