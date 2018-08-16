
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSArrayObjectDeleter.h
Description: Provide an Auto object for deleting arrays.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#ifndef __OS_ARRAY_OBJECT_DELETER_H__
#define __OS_ARRAY_OBJECT_DELETER_H__

#include "MyAssert.h"

template <class T>
/* 由类对象组成的数组的删除类,这个很重要,下面有两个具体的类定义 */
class OSArrayObjectDeleter
{
    public:
        OSArrayObjectDeleter(T* victim) : fT(victim)  {}
        ~OSArrayObjectDeleter() { delete [] fT; }
        
		//modifiers
        void ClearObject() { fT = NULL; }

        void SetObject(T* victim) 
        {
            Assert(fT == NULL);
            fT = victim; 
        }

		//accessors
        T* GetObject() { return fT; }
        
		//operators
        operator T*() { return fT; }
    
    private:
    
        T* fT;
};


template <class T>
/* 删除指针的类 */
class OSPtrDeleter
{
    public:
        OSPtrDeleter(T* victim) : fT(victim)  {}
        ~OSPtrDeleter() { delete fT; }

        //modifiers
        void ClearObject() { fT = NULL; }

        void SetObject(T* victim) 
        {   Assert(fT == NULL);
            fT = victim; 
        }
            
    private:
    
        T* fT;
};


typedef OSArrayObjectDeleter<char*> OSCharPointerArrayDeleter;
typedef OSArrayObjectDeleter<char> OSCharArrayDeleter;

#endif //__OS_OBJECT_ARRAY_DELETER_H__
