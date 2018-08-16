
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSExpirationDate.cpp
Description: Routine that checks to see if software is expired.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include "QTSSExpirationDate.h"

#include "MyAssert.h"
#include "OSHeaders.h"
#include "SafeStdLib.h"
#include <time.h>


Bool16  QTSSExpirationDate::sIsExpirationEnabled = false;
//must be in "5/12/1998" format, "m/d/4digityear"
char*   QTSSExpirationDate::sExpirationDate = "3/15/2002";

//打印软件到期时间
void QTSSExpirationDate::PrintExpirationDate()
{
    if (sIsExpirationEnabled)
        qtss_printf("Software expires on: %s\n", sExpirationDate);
}

//将软件到期时间存入入参指定缓存
void QTSSExpirationDate::sPrintExpirationDate(char* ioExpireMessage)
{
    if (sIsExpirationEnabled)
        qtss_sprintf(ioExpireMessage, "Software expires on: %s\n", sExpirationDate);
}

/* 判断软件是否过期? 若为true,就过期了;若为false,不过期 */
Bool16 QTSSExpirationDate::IsSoftwareExpired()
{
	//假如不让使用软件过期提示功能,返回false
    if (!sIsExpirationEnabled)
        return false;
    
	//从设定的软件到期时间中提取出年/月/日信息,若提取出错,返回true
    SInt32 expMonth, expDay, expYear;
    if (EOF == ::sscanf(sExpirationDate, "%ld/%ld/%ld", &expMonth, &expDay, &expYear))
    {
        Assert(false);
        return true;
    }
    
    //sanity checks,进行合法性检查,如出错,返回true
    Assert((expMonth > 0) && (expMonth <= 12));
    if ((expMonth <= 0) || (expMonth > 12))
        return true;
    
    Assert((expDay > 0) && (expDay <= 31));
    if ((expDay <= 0) || (expDay > 31))
        return true;
        
    Assert(expYear >= 1998);
    if (expYear < 1998)
        return true;
    
	//获取当前时间
    time_t theCurrentTime = ::time(NULL);
    Assert(theCurrentTime != -1);
    if (theCurrentTime == -1)
        return true;
     
	//将当前时间转换为当前本地时间
    struct tm  timeResult;
    struct tm* theLocalTime = qtss_localtime(&theCurrentTime, &timeResult);
    Assert(theLocalTime != NULL);
    if (theLocalTime == NULL)
        return true;
        
    if (expYear > (theLocalTime->tm_year + 1900))
        return false;//ok
    if (expYear < (theLocalTime->tm_year + 1900))
        return true;//expired

    if (expMonth > (theLocalTime->tm_mon + 1))
        return false;//ok
    if (expMonth < (theLocalTime->tm_mon + 1))
        return true;//expired

    if (expDay > theLocalTime->tm_mday)
        return false;//ok
    else
        return true;//expired
}

