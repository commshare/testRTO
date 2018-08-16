
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 revision.h
Description: define the version number of tau streaming server on Linux platform.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


// use no http/rtsp tspecial chars in kVersionString and kBuildString defines
#define kVersionString "5.5.1"  //指出本源代码的版本号
#define kBuildString "489.8"

// Use kCommentString for seed or other release info 
// Do not use '(' or ')' in the kCommentString
// form = token1/info; token2/info;
// example "Release/public seed 1; Event/Big Event; state/half-baked"
#define kCommentString "Release/Darwin; " 
