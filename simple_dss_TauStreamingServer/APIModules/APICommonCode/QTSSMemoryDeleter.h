
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSMemoryDeleter.h
Description: Auto object for deleting memory allocated by QTSS API callbacks,such as QTSS_GetValueAsString
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef __QTSS_MEMORY_DELETER_H__
#define __QTSS_MEMORY_DELETER_H__

#include "MyAssert.h"
#include "QTSS.h"

template <class T>
class QTSSMemoryDeleter
{
    public:
        QTSSMemoryDeleter(T* victim) : fT(victim)  {}
        ~QTSSMemoryDeleter() { QTSS_Delete(fT); }
        
        void ClearObject() { fT = NULL; }

        void SetObject(T* victim) 
        {
            Assert(fT == NULL);
            fT = victim; 
        }
        T* GetObject() { return fT; }
        
        operator T*() { return fT; }
    
    private:
    
        T* fT;
};

typedef QTSSMemoryDeleter<char> QTSSCharArrayDeleter;

#endif //__QTSS_MEMORY_DELETER_H__


