
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

//��ӡ�������ʱ��
void QTSSExpirationDate::PrintExpirationDate()
{
    if (sIsExpirationEnabled)
        qtss_printf("Software expires on: %s\n", sExpirationDate);
}

//���������ʱ��������ָ������
void QTSSExpirationDate::sPrintExpirationDate(char* ioExpireMessage)
{
    if (sIsExpirationEnabled)
        qtss_sprintf(ioExpireMessage, "Software expires on: %s\n", sExpirationDate);
}

/* �ж�����Ƿ����? ��Ϊtrue,�͹�����;��Ϊfalse,������ */
Bool16 QTSSExpirationDate::IsSoftwareExpired()
{
	//���粻��ʹ�����������ʾ����,����false
    if (!sIsExpirationEnabled)
        return false;
    
	//���趨���������ʱ������ȡ����/��/����Ϣ,����ȡ����,����true
    SInt32 expMonth, expDay, expYear;
    if (EOF == ::sscanf(sExpirationDate, "%ld/%ld/%ld", &expMonth, &expDay, &expYear))
    {
        Assert(false);
        return true;
    }
    
    //sanity checks,���кϷ��Լ��,�����,����true
    Assert((expMonth > 0) && (expMonth <= 12));
    if ((expMonth <= 0) || (expMonth > 12))
        return true;
    
    Assert((expDay > 0) && (expDay <= 31));
    if ((expDay <= 0) || (expDay > 31))
        return true;
        
    Assert(expYear >= 1998);
    if (expYear < 1998)
        return true;
    
	//��ȡ��ǰʱ��
    time_t theCurrentTime = ::time(NULL);
    Assert(theCurrentTime != -1);
    if (theCurrentTime == -1)
        return true;
     
	//����ǰʱ��ת��Ϊ��ǰ����ʱ��
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

