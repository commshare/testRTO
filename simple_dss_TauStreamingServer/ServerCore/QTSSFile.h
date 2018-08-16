
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSFile.h
Description: Class QTSSFile definition .
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include "QTSSDictionary.h"
#include "QTSSModule.h"

#include "OSFileSource.h"
#include "EventContext.h"

class QTSSFile : public QTSSDictionary
{
    public:
    
        QTSSFile();
        virtual ~QTSSFile() {}
        
        static void     Initialize();
        
        //
        // Opening & Closing
        QTSS_Error          Open(char* inPath, QTSS_OpenFileFlags inFlags);
        void                Close();
        
        //
        // Implementation of stream functions.
        virtual QTSS_Error  Read(void* ioBuffer, UInt32 inLen, UInt32* outLen);
        
        virtual QTSS_Error  Seek(UInt64 inNewPosition);
        
        virtual QTSS_Error  Advise(UInt64 inPosition, UInt32 inAdviseSize);
        
        virtual QTSS_Error  RequestEvent(QTSS_EventType inEventMask);
        
    private:

        QTSSModule* fModule; //模块指针
        UInt64      fPosition;
        QTSSFile*   fThisPtr; //这个很奇怪!
        
        //
        // File attributes
        UInt64      fLength;
        time_t      fModDate;

		//refer to QTSSDictionary.h line 252
		//refer to QTSSFile.h line 39
        static QTSSAttrInfoDict::AttrInfo   sAttributes[]; 
};

