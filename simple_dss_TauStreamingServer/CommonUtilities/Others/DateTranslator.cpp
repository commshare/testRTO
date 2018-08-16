/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 DateTranslator.h
Description: Efficient routines & data structures for converting from
RFC 1123 compliant date strings to local file system dates & vice versa.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 



#include <time.h>
#include "DateTranslator.h"
#include "OSHeaders.h"
#include "OS.h"  /* 涉及时间格式的处理 */
#include "StringParser.h"
#include "StrPtrLen.h"



// If you assign values of 0 - 25 for all the letters, and sum up the values of
// the letters in each month, you get a table that looks like this. For instance,
// "Jul" = 9 + 20 + 11 = 40. The value of July in a C tm struct is 6, so position
// 40 = 6 in this array.

const UInt32 kMonthHashTable[] =
{
    12, 12, 12, 12, 12, 12, 12, 12, 12, 11,     // 0 - 9
    1,  12, 12, 12, 12, 12, 12, 12, 12, 12,     // 10 - 19
    12, 12, 0,  12, 12, 12, 7,  12, 12, 2,      // 20 - 29
    12, 12, 3,  12, 12, 9,  4,  8,  12, 12,     // 30 - 39
    6,  12, 5,  12, 12, 12, 12, 12, 10, 12      // 40 - 49
};
const UInt32 kMonthHashTableSize = 49;


/* used in RTSPRequest::ParseIfModSinceHeader() */
SInt64  DateTranslator::ParseDate(StrPtrLen* inDateString)
{
    //SEE RFC 1123 for details on the date string format
    //ex: Mon, 04 Nov 1996 21:42:17 GMT

    // Parse the date buffer, filling out a tm struct
    struct tm theDateStruct;
    ::memset(&theDateStruct, 0, sizeof(theDateStruct));

    // All RFC 1123 dates are the same length.
    if (inDateString->Len != DateBuffer::kDateBufferLen)
        return 0;
    
    StringParser theDateParser(inDateString);
        
    // the day of the week is redundant... we can skip it!
    theDateParser.ConsumeLength(NULL, 5);
    
    // We are at the date now.
    theDateStruct.tm_mday = theDateParser.ConsumeInteger(NULL);
    theDateParser.ConsumeWhitespace();
    
    // We are at the month now. Use our hand-crafted perfect hash table
    // to get the right value to place in the tm struct
    if (theDateParser.GetDataRemaining() < 4)
        return 0;
    
    UInt32 theIndex =   ConvertCharToMonthTableIndex(theDateParser.GetCurrentPosition()[0]) +
                        ConvertCharToMonthTableIndex(theDateParser.GetCurrentPosition()[1]) +
                        ConvertCharToMonthTableIndex(theDateParser.GetCurrentPosition()[2]);
    
    if (theIndex > kMonthHashTableSize)
        return 0;
        
    theDateStruct.tm_mon = kMonthHashTable[theIndex];

    // If the month is illegal, return an error
    if (theDateStruct.tm_mon >= 12)
        return 0;
        
    // Skip over the date
    theDateParser.ConsumeLength(NULL, 4);   
    
    // Grab the year (years since 1900 is what the tm struct wants)
    theDateStruct.tm_year = theDateParser.ConsumeInteger(NULL) - 1900;
    theDateParser.ConsumeWhitespace();

    // Now just grab hour, minute, second
    theDateStruct.tm_hour = theDateParser.ConsumeInteger(NULL);
    theDateStruct.tm_hour += OS::GetGMTOffset();
    
    theDateParser.ConsumeLength(NULL, 1); //skip over ':'   

    theDateStruct.tm_min = theDateParser.ConsumeInteger(NULL);
    theDateParser.ConsumeLength(NULL, 1); //skip over ':'   

    theDateStruct.tm_sec = theDateParser.ConsumeInteger(NULL);

    // Ok, we've filled out the tm struct completely, now convert it to a time_t
    time_t theTime = ::mktime(&theDateStruct);
    return (SInt64)theTime * 1000; // convert to a time value in our timebase.
}

/* 当入参inDate存在时,将该时间调整为GMT;当入参inDate不存在时,将local当前时间调整为GMT.再将GMT时间以指定格式"Mon, 04 Nov 1996 21:42:17 GMT"存入入参inDateBuffer */
void DateTranslator::UpdateDateBuffer(DateBuffer* inDateBuffer, const SInt64& inDate, time_t gmtoffset)
{
    if (inDateBuffer == NULL)
        return;

    struct tm* gmt = NULL;
    struct tm  timeResult;
    
    if (inDate == 0)
    {
        time_t calendarTime = ::time(NULL) + gmtoffset;
        gmt = ::qtss_gmtime(&calendarTime, &timeResult);
    }
    else
    {
        time_t convertedTime = (time_t)(inDate / (SInt64)1000) + gmtoffset ; // Convert from msec to sec and from local time to GMT time
        gmt = ::qtss_gmtime(&convertedTime, &timeResult);
    }
     
	//SEE RFC 1123 for details on the date string format
	//ex: Mon, 04 Nov 1996 21:42:17 GMT
    Assert(gmt != NULL); //is it safe to assert this?
    size_t size  = 0;
    if (0 == gmtoffset)
        size = qtss_strftime(   inDateBuffer->fDateBuffer, sizeof(inDateBuffer->fDateBuffer),
                            "%a, %d %b %Y %H:%M:%S GMT", gmt);
                            
    Assert(size == DateBuffer::kDateBufferLen);
}

void DateBuffer::InexactUpdate()
{
    SInt64 theCurTime = OS::Milliseconds();
    if ((fLastDateUpdate == 0) || ((fLastDateUpdate + kUpdateInterval) < theCurTime))
    {
        fLastDateUpdate = theCurTime;
        this->Update(0);
    }
}
