
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



#ifndef _DATE_TRANSLATOR_H_
#define _DATE_TRANSLATOR_H_

#include <ctype.h>
#include <time.h>
#include "StrPtrLen.h"



class DateBuffer; //explicit code see below

class DateTranslator
{
    public:

        // this updates the DateBuffer to be the current date / time.
        // If you wish to set the DateBuffer to a particular date, pass in that date.
        // Dates should be in the OS.h compliant format
		/* used in RTSPRequestStream::ReadRequest() */
		/* 当入参inDate存在时,将该时间调整为GMT;当入参inDate不存在时,将local当前时间调整为GMT.再将GMT时间
		   以指定格式"Mon, 04 Nov 1996 21:42:17 GMT"存入入参inDateBuffer */
        static void UpdateDateBuffer(DateBuffer* inDateBuffer, const SInt64& inDate, time_t gmtoffset = 0);
        
        //Given an HTTP/1.1 compliant date string (in one of the three 1.1 formats)
        //this returns an OS.h compliant date/time value.
        static SInt64   ParseDate(StrPtrLen* inDateString);
        
    private:

        static UInt32 ConvertCharToMonthTableIndex(int inChar)
        {
            return (UInt32)(toupper(inChar) - 'A'); // Convert to a value between 0 - 25
        }
};

class DateBuffer
{
public:

    // This class provides no protection against being accessed from multiple threads
    // simultaneously. Update & InexactUpdate rewrite the date buffer, so care should
    // be taken to protect against simultaneous access.

    DateBuffer() : fLastDateUpdate(0) { fDateBuffer[0] = 0; }
    ~DateBuffer() {}
    
    //SEE RFC 1123 for details on the date string format
    //ex: Mon, 04 Nov 1996 21:42:17 GMT
    
    //RFC 1123 date strings are always of this length
	/* 由上面的例子可以数出来 */
    enum
    {
        kDateBufferLen = 29
    };
    
    // Updates this date buffer to reflect the current time.
    // If a date is provided, this updates the DateBuffer to be that date.
    void Update(const SInt64& inDate)           { DateTranslator::UpdateDateBuffer(this, inDate); }
    
    // Updates this date buffer to reflect the current time, with a certain degree
    // of inexactitude (the range of error is defined by the kUpdateInterval value)
    void InexactUpdate();
    
    //returns a NULL terminated C-string always of kHTTPDateLen length.
    char *GetDateBuffer()   { return fDateBuffer; }

private:

    enum
    {
        kUpdateInterval = 60000 // Update every minute
    };

    //+1 for terminator +1 for padding
    char    fDateBuffer[kDateBufferLen + 2];
    SInt64  fLastDateUpdate;
    
    friend class DateTranslator;
};


#endif


