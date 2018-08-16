
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSS_Private.h
Description: Implementation-specific structures and typedefs used by the
             implementation of QTSS API in the Darwin Streaming Server.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 

#ifndef QTSS_PRIVATE_H
#define QTSS_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "OSHeaders.h" 
#include "QTSS.h"

class QTSSModule;
class Task; 

/* 定义两种类型的函数指针 */
typedef QTSS_Error  (*QTSS_CallbackProcPtr)(...);   //QTSS_Error是Sint32类型,见QTSS.h定义,此处强制类型转换为QTSS_Error类型
typedef void*       (*QTSS_CallbackPtrProcPtr)(...);

/* 所有的callback routines的索引列表,在下面的QTSS_PrivateArgs结构体中使用 */
enum
{
    // INDEXES FOR EACH CALLBACK ROUTINE. 
	// all callback routines' declarations refer to QTSS.h, yet whose code definitions refer to QTSS_Private.cpp.
    // Addresses of the callback routines get placed in an array. 
    // IMPORTANT: When adding new callbacks, add only to the end of the list and increment the 
    //            kLastCallback value. Inserting or changing the index order will break dynamic modules
    //            built with another release.
    
    kNewCallback                    = 0,
    kDeleteCallback                 = 1,
    kMillisecondsCallback           = 2,
    kConvertToUnixTimeCallback      = 3,
    kAddRoleCallback                = 4,
    kAddAttributeCallback           = 5,
    kIDForTagCallback               = 6,
    kGetAttributePtrByIDCallback    = 7,
    kGetAttributeByIDCallback       = 8,
    kSetAttributeByIDCallback       = 9,
    kWriteCallback                  = 10,
    kWriteVCallback                 = 11,
    kFlushCallback                  = 12,
    kAddServiceCallback             = 13,
    kIDForServiceCallback           = 14,
    kDoServiceCallback              = 15,
    kSendRTSPHeadersCallback        = 16,
    kAppendRTSPHeadersCallback      = 17,
    kSendStandardRTSPCallback       = 18,
    kAddRTPStreamCallback           = 19,
    kPlayCallback                   = 20,
    kPauseCallback                  = 21,
    kTeardownCallback               = 22,
    kRequestEventCallback           = 23,
    kSetIdleTimerCallback           = 24,
    kOpenFileObjectCallback         = 25,
    kCloseFileObjectCallback        = 26,
    kReadCallback                   = 27,
    kSeekCallback                   = 28,
    kAdviseCallback                 = 29,
    kGetNumValuesCallback           = 30,
    kGetNumAttributesCallback       = 31,
    kSignalStreamCallback           = 32,
    kCreateSocketStreamCallback     = 33,
    kDestroySocketStreamCallback    = 34,
    kAddStaticAttributeCallback     = 35,
    kAddInstanceAttributeCallback   = 36,
    kRemoveInstanceAttributeCallback= 37,
    kGetAttrInfoByIndexCallback     = 38,
    kGetAttrInfoByNameCallback      = 39,
    kGetAttrInfoByIDCallback        = 40,
    kGetValueAsStringCallback       = 41,
    kTypeToTypeStringCallback       = 42,
    kTypeStringToTypeCallback       = 43,
    kStringToValueCallback          = 44,       
    kValueToStringCallback          = 45,       
    kRemoveValueCallback            = 46,
    kRequestGlobalLockCallback      = 47, 
    kIsGlobalLockedCallback         = 48, 
    kUnlockGlobalLock               = 49, 
    kAuthenticateCallback           = 50,
    kAuthorizeCallback              = 51,   
    kRefreshTimeOutCallback         = 52,
    kCreateObjectValueCallback      = 53,
    kCreateObjectTypeCallback       = 54,
    kLockObjectCallback             = 55,
    kUnlockObjectCallback           = 56,
    kSetAttributePtrCallback        = 57,
    kSetIntervalRoleTimerCallback   = 58,
    kLockStdLibCallback             = 59,
    kUnlockStdLibCallback           = 60,
    kLastCallback                   = 61
};

typedef struct {
    // Callback function long type pointer array
    QTSS_CallbackProcPtr addr [kLastCallback]; //回调函数指针数组,这每个回调函数指针的实现源码在哪里?参见QTSSCallbacks.cpp
} QTSS_Callbacks, *QTSS_CallbacksPtr;


/* 注意这个结构体内容极其丰富,值得仔细体会!! */
typedef struct
{
    UInt32                  inServerAPIVersion;
    QTSS_CallbacksPtr       inCallbacks;  //见上结构体定义,实际上上面的定义就是在此处使用!
    QTSS_StreamRef          inErrorLogStream;/* 错误日志流 */
    UInt32                  outStubLibraryVersion;
    QTSS_DispatchFuncPtr    outDispatchFunction; /* 分发函数指针 */
    
} QTSS_PrivateArgs, *QTSS_PrivateArgsPtr;

// this structure is setup in each thread before invoking a module in a role. Sometimes this info. helps callback implementation
/* 当执行回调函数,每个thread调用一个module时要用到 */
typedef struct
{
    QTSSModule* curModule;  // 当前模块指针
    QTSS_Role   curRole;    // 当前引用的Role
    Task*       curTask;    // 当前要从事的task
    Bool16      eventRequested;  /* request event? */
    Bool16      globalLockRequested;    // request event with global lock?
    Bool16      isGlobalLocked; /* 现在是global locked? */
    SInt64      idleTime;   // If a module has requested idle time. 让模块闲置的时间
    
} QTSS_ModuleState, *QTSS_ModuleStatePtr; /* needed in RTPSession::run() */


/* 定义GetErrorLogStream()函数 */
typedef QTSS_StreamRef  GetErrorLogStream(); //refer to QTSS.h with line 982


#ifdef __cplusplus
}
#endif

#endif
