
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSMessages.h
Description: This global dictionary provides a central mapping from message
             names to actual text messages, stored in the provided prefs source.
			 This allows the whole server to be easily localizeable.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef __QTSSMESSAGES_H__
#define __QTSSMESSAGES_H__

#include "QTSS.h"
#include "QTSSDictionary.h"
//#include "PrefsSource.h"

class QTSSMessages : public QTSSDictionary
{
    public:
    
        // INITIALIZE
        //
        // This function sets up the dictionary map. Must be called before instantiating
        // the first RTSPMessages object.
    
        static void Initialize();
    
        QTSSMessages(/*PrefsSource* inMessages*/);
        virtual ~QTSSMessages() {}
        

        //Use the standard GetAttribute method in QTSSDictionary to retrieve messages
        
    private:
    
        enum
        {
            kNumMessages = 74 // 0 based count so it is one more than last message index number
        };
    
        static char*        sMessagesKeyStrings[];
        static char*        sMessages[];
};


#endif // __QTSSMESSAGES_H__
