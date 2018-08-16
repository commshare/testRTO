
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSS_Private.cpp
Description: Code for stub library and stub callback functions whose definitions please see in QTSS.h.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "QTSS.h" //all definitions of  data structure types and callback routines, in which just callback routines definitions used here
#include "QTSS_Private.h"
#include "SafeStdLib.h"



static QTSS_CallbacksPtr    sCallbacks = NULL; //指向回调例程指针数组的指针,初始化为空指针,QTSS_CallbacksPtr定义见QTSS_Private.h 
static QTSS_StreamRef       sErrorLogStream = NULL;//声明静态变量可作为全局变量,QTSS_StreamRef定义见QTSS.h

/* 另见QTSSModule::SetupModule() */
/* 由第一个入参配置两个静态变量值,同时用第二个入参配置第一个入参中的成员值 */
QTSS_Error _stublibrary_main(void* inPrivateArgs, QTSS_DispatchFuncPtr inDispatchFunc)
{
	/* 从第一个入参获取这个非常重要的结构体指针 */
    QTSS_PrivateArgsPtr theArgs = (QTSS_PrivateArgsPtr)inPrivateArgs; //QTSS_PrivateArgsPtr是指向结构体的指针，见相应头文件定义
    
    // Setup        

	/* 由第一个入参配置回调函数指针数组 */
    sCallbacks = theArgs->inCallbacks;    //theArgs是指向结构体的指针，inCallbacks是指向回调例程指针数组的指针

	/* 由第一个入参配置错误日志流函数 */
    sErrorLogStream = theArgs->inErrorLogStream; //inErrorLogStream是QTSS_StreamRef类型的指针,用于标示一个特定的流
    
    // Send requested information back to the server
    /* 配置第一个入参中的成员 */
    theArgs->outStubLibraryVersion = QTSS_API_VERSION; //参见QTSS.h定义,0x00040000

	/* 用第二个入参(它是第三方Module的分发函数指针/地址)配置第一个入参中的成员 */
    theArgs->outDispatchFunction = inDispatchFunc; //由函数参数inDispatchFunc指针输出QTSS_DispatchFuncPtr类型信息
    
    return QTSS_NoErr;
}

// STUB FUNCTION DEFINITIONS   // QTSS Utility Callback routines

void*           QTSS_New(FourCharCode inMemoryIdentifier, UInt32 inSize) //参见开发文档中对该回调函数的定义,分配内存,FourCharCode定义见OSHeaders.h,unsigned long
{
    return (void *) ((QTSS_CallbackPtrProcPtr) sCallbacks->addr [kNewCallback]) (inMemoryIdentifier, inSize);//回调例程指针数组的第一个分量,强制转化为空指针
}

void            QTSS_Delete(void* inMemory) //参见开发文档中对该回调函数的定义,释放内存
{
    (sCallbacks->addr [kDeleteCallback]) (inMemory);//指向回调例程指针数组的第二个分量
}

SInt64          QTSS_Milliseconds(void)//参见开发文档中对该回调函数的定义,获取服务器时钟内部当前值
{
    SInt64 outMilliseconds = 0;
    (sCallbacks->addr [kMillisecondsCallback]) (&outMilliseconds);//指向第三个分量,服务器时钟内部当前值传递到outMilliseconds地址
    return outMilliseconds;
}

time_t          QTSS_MilliSecsTo1970Secs(SInt64 inQTSS_MilliSeconds) //参见开发文档中对该回调函数的定义,将来自服务器内部时钟的时间值转换为当前时间
{
    time_t outSeconds = 0;
    (sCallbacks->addr [kConvertToUnixTimeCallback]) (&inQTSS_MilliSeconds, &outSeconds);//指向第四个分量
    return outSeconds;
}

// STARTUP ROUTINES
    
QTSS_Error  QTSS_AddRole(QTSS_Role inRole) //参见开发文档中对该回调函数的定义,指定即将被加入的角色
{
    return (sCallbacks->addr [kAddRoleCallback]) (inRole);  //指向第五个分量
}

// DICTIONARY ROUTINES 
//QTSS OBJECT CALLBACK ROUNTINES

QTSS_Error  QTSS_CreateObjectType(QTSS_ObjectType* outType)
{
    return (sCallbacks->addr [kCreateObjectTypeCallback]) (outType);    
}

QTSS_Error  QTSS_CreateObjectValue (QTSS_Object inDictionary, QTSS_AttributeID inID, QTSS_ObjectType inType, UInt32* outIndex, QTSS_Object* outCreatedObject)
{
    return (sCallbacks->addr [kCreateObjectValueCallback]) (inDictionary, inID, inType, outIndex, outCreatedObject);    
}

//QTSS ATTRIBUTE CALLBACK ROUNTINES

QTSS_Error  QTSS_AddAttribute(QTSS_ObjectType inType, const char* inTag, void* inUnused)
{
    return (sCallbacks->addr [kAddAttributeCallback]) (inType, inTag, inUnused);    
}

QTSS_Error  QTSS_AddStaticAttribute(QTSS_ObjectType inObjectType, char* inAttrName, void* inUnused, QTSS_AttrDataType inAttrDataType)
{
    return (sCallbacks->addr [kAddStaticAttributeCallback]) (inObjectType, inAttrName, inUnused, inAttrDataType);   
}

QTSS_Error  QTSS_AddInstanceAttribute(QTSS_Object inObject, char* inAttrName, void* inUnused, QTSS_AttrDataType inAttrDataType)
{
    return (sCallbacks->addr [kAddInstanceAttributeCallback]) (inObject, inAttrName, inUnused, inAttrDataType); 
}

QTSS_Error  QTSS_IDForAttr(QTSS_ObjectType inType, const char* inTag, QTSS_AttributeID* outID)
{
    return (sCallbacks->addr [kIDForTagCallback]) (inType, inTag, outID);   
}

QTSS_Error QTSS_GetAttrInfoByIndex(QTSS_Object inObject, UInt32 inIndex, QTSS_Object* outAttrInfoObject)
{
    return (sCallbacks->addr [kGetAttrInfoByIndexCallback]) (inObject, inIndex, outAttrInfoObject); 
}

QTSS_Error QTSS_GetAttrInfoByID(QTSS_Object inObject, QTSS_AttributeID inAttrID, QTSS_Object* outAttrInfoObject)
{
    return (sCallbacks->addr [kGetAttrInfoByIDCallback]) (inObject, inAttrID, outAttrInfoObject);   
}

QTSS_Error QTSS_GetAttrInfoByName(QTSS_Object inObject, char* inAttrName, QTSS_Object* outAttrInfoObject)
{
    return (sCallbacks->addr [kGetAttrInfoByNameCallback]) (inObject, inAttrName, outAttrInfoObject);   
}

QTSS_Error  QTSS_GetValuePtr (QTSS_Object inDictionary, QTSS_AttributeID inID, UInt32 inIndex, void** outBuffer, UInt32* outLen)
{
    return (sCallbacks->addr [kGetAttributePtrByIDCallback]) (inDictionary, inID, inIndex, outBuffer, outLen);  
}

QTSS_Error  QTSS_GetValue (QTSS_Object inDictionary, QTSS_AttributeID inID, UInt32 inIndex, void* ioBuffer, UInt32* ioLen)
{
    return (sCallbacks->addr [kGetAttributeByIDCallback]) (inDictionary, inID, inIndex, ioBuffer, ioLen);   
}

QTSS_Error QTSS_GetValueAsString (QTSS_Object inObject, QTSS_AttributeID inID, UInt32 inIndex, char** outString)
{
    return (sCallbacks->addr [kGetValueAsStringCallback]) (inObject, inID, inIndex, outString); 
}

QTSS_Error  QTSS_TypeStringToType(const char* inTypeString, QTSS_AttrDataType* outType)
{
    return (sCallbacks->addr [kTypeStringToTypeCallback]) (inTypeString, outType);  
}

QTSS_Error  QTSS_TypeToTypeString(const QTSS_AttrDataType inType, char** outTypeString)
{
    return (sCallbacks->addr [kTypeToTypeStringCallback]) (inType, outTypeString);  
}

QTSS_Error  QTSS_StringToValue(const char* inValueAsString, const QTSS_AttrDataType inType, void* ioBuffer, UInt32* ioBufSize)
{
    return (sCallbacks->addr [kStringToValueCallback]) (inValueAsString, inType, ioBuffer, ioBufSize);  
}

QTSS_Error  QTSS_ValueToString(const void* inValue, const UInt32 inValueLen, const QTSS_AttrDataType inType, char** outString)
{
    return (sCallbacks->addr [kValueToStringCallback]) (inValue, inValueLen, inType, outString);    
}

QTSS_Error  QTSS_SetValue (QTSS_Object inDictionary, QTSS_AttributeID inID,UInt32 inIndex,  const void* inBuffer,  UInt32 inLen)
{
    return (sCallbacks->addr [kSetAttributeByIDCallback]) (inDictionary, inID, inIndex, inBuffer, inLen);   
}

QTSS_Error  QTSS_SetValuePtr (QTSS_Object inDictionary, QTSS_AttributeID inID, const void* inBuffer,  UInt32 inLen)
{
    return (sCallbacks->addr [kSetAttributePtrCallback]) (inDictionary, inID, inBuffer, inLen); 
}

QTSS_Error  QTSS_GetNumValues (QTSS_Object inObject, QTSS_AttributeID inID, UInt32* outNumValues)
{
    return (sCallbacks->addr [kGetNumValuesCallback]) (inObject, inID, outNumValues);   
}

QTSS_Error  QTSS_GetNumAttributes (QTSS_Object inObject, UInt32* outNumValues)
{
    return (sCallbacks->addr [kGetNumAttributesCallback]) (inObject, outNumValues); 
}

QTSS_Error  QTSS_RemoveValue (QTSS_Object inObject, QTSS_AttributeID inID, UInt32 inIndex)
{
    return (sCallbacks->addr [kRemoveValueCallback]) (inObject, inID, inIndex); 
}

QTSS_Error QTSS_RemoveInstanceAttribute(QTSS_Object inObject, QTSS_AttributeID inID)
{
    return (sCallbacks->addr [kRemoveInstanceAttributeCallback]) (inObject, inID);  
}

// STREAM CALLBACK ROUTINES

QTSS_Error  QTSS_Write(QTSS_StreamRef inStream, const void* inBuffer, UInt32 inLen, UInt32* outLenWritten, UInt32 inFlags)
{
    return (sCallbacks->addr [kWriteCallback]) (inStream, inBuffer, inLen, outLenWritten, inFlags); 
}

QTSS_Error  QTSS_WriteV(QTSS_StreamRef inStream, iovec* inVec, UInt32 inNumVectors, UInt32 inTotalLength, UInt32* outLenWritten)
{
    return (sCallbacks->addr [kWriteVCallback]) (inStream, inVec, inNumVectors, inTotalLength, outLenWritten);  
}

QTSS_Error  QTSS_Flush(QTSS_StreamRef inStream)
{
    return (sCallbacks->addr [kFlushCallback]) (inStream);  
}

QTSS_Error  QTSS_Read(QTSS_StreamRef inRef, void* ioBuffer, UInt32 inBufLen, UInt32* outLengthRead)
{
    return (sCallbacks->addr [kReadCallback]) (inRef, ioBuffer, inBufLen, outLengthRead);       
}

QTSS_Error  QTSS_Seek(QTSS_StreamRef inRef, UInt64 inNewPosition)
{
    return (sCallbacks->addr [kSeekCallback]) (inRef, inNewPosition);
}

QTSS_Error  QTSS_Advise(QTSS_StreamRef inRef, UInt64 inPosition, UInt32 inAdviseSize)
{
    return (sCallbacks->addr [kAdviseCallback]) (inRef, inPosition, inAdviseSize);      
}

// SERVICE CALLBACK ROUTINES

QTSS_Error QTSS_AddService(const char* inServiceName, QTSS_ServiceFunctionPtr inFunctionPtr)
{
    return (sCallbacks->addr [kAddServiceCallback]) (inServiceName, inFunctionPtr);     
}

QTSS_Error QTSS_IDForService(const char* inTag, QTSS_ServiceID* outID)
{
    return (sCallbacks->addr [kIDForServiceCallback]) (inTag, outID);   
}

QTSS_Error QTSS_DoService(QTSS_ServiceID inID, QTSS_ServiceFunctionArgsPtr inArgs)
{
    return (sCallbacks->addr [kDoServiceCallback]) (inID, inArgs);  
}

// RTSP HEAD CALLBACK ROUTINES

QTSS_Error QTSS_SendRTSPHeaders(QTSS_RTSPRequestObject inRef)
{
    return (sCallbacks->addr [kSendRTSPHeadersCallback]) (inRef);       
}

QTSS_Error QTSS_AppendRTSPHeader(QTSS_RTSPRequestObject inRef, QTSS_RTSPHeader inHeader, const char* inValue, UInt32 inValueLen)
{
    return (sCallbacks->addr [kAppendRTSPHeadersCallback]) (inRef, inHeader, inValue, inValueLen);      
}

QTSS_Error QTSS_SendStandardRTSPResponse(QTSS_RTSPRequestObject inRTSPRequest, QTSS_Object inRTPInfo, UInt32 inFlags)
{
    return (sCallbacks->addr [kSendStandardRTSPCallback]) (inRTSPRequest, inRTPInfo, inFlags);      
}

// RTP CALLBACK ROUTINES

QTSS_Error QTSS_AddRTPStream(QTSS_ClientSessionObject inClientSession, QTSS_RTSPRequestObject inRTSPRequest, QTSS_RTPStreamObject* outStream, QTSS_AddStreamFlags inFlags)
{
    return (sCallbacks->addr [kAddRTPStreamCallback]) (inClientSession, inRTSPRequest, outStream, inFlags);     
}

QTSS_Error QTSS_Play(QTSS_ClientSessionObject inClientSession, QTSS_RTSPRequestObject inRTSPRequest, QTSS_PlayFlags inPlayFlags)
{
    return (sCallbacks->addr [kPlayCallback]) (inClientSession, inRTSPRequest, inPlayFlags);        
}

QTSS_Error QTSS_Pause(QTSS_ClientSessionObject inClientSession)
{
    return (sCallbacks->addr [kPauseCallback]) (inClientSession);       
}

QTSS_Error QTSS_Teardown(QTSS_ClientSessionObject inClientSession)
{
    return (sCallbacks->addr [kTeardownCallback]) (inClientSession);        
}

QTSS_Error QTSS_RefreshTimeOut(QTSS_ClientSessionObject inClientSession)
{
    return (sCallbacks->addr [kRefreshTimeOutCallback]) (inClientSession);
}


// FILE SYSTEM CALLBACK ROUTINES

QTSS_Error  QTSS_OpenFileObject(char* inPath, QTSS_OpenFileFlags inFlags, QTSS_Object* outFileObject)
{
    return (sCallbacks->addr [kOpenFileObjectCallback]) (inPath, inFlags, outFileObject);       
}

QTSS_Error  QTSS_CloseFileObject(QTSS_Object inFileObject)
{
    return (sCallbacks->addr [kCloseFileObjectCallback]) (inFileObject);        
}

// SOCKET ROUTINES

QTSS_Error  QTSS_CreateStreamFromSocket(int inFileDesc, QTSS_StreamRef* outStream) //参见QTSS.h定义
{
    return (sCallbacks->addr [kCreateSocketStreamCallback]) (inFileDesc, outStream);        
}

QTSS_Error  QTSS_DestroySocketStream(QTSS_StreamRef inStream)
{
    return (sCallbacks->addr [kDestroySocketStreamCallback]) (inStream);        

}

// ASYNC I/O STREAM CALLBACK ROUTINES

QTSS_Error  QTSS_RequestEvent(QTSS_StreamRef inStream, QTSS_EventType inEventMask)
{
    return (sCallbacks->addr [kRequestEventCallback]) (inStream, inEventMask);      
}

QTSS_Error  QTSS_SignalStream(QTSS_StreamRef inStream)
{
    return (sCallbacks->addr [kSignalStreamCallback]) (inStream);       
}

QTSS_Error  QTSS_SetIdleTimer(SInt64 inIdleMsec)
{
    return (sCallbacks->addr [kSetIdleTimerCallback]) (inIdleMsec);     
}

QTSS_Error  QTSS_SetIntervalRoleTimer(SInt64 inIdleMsec)
{
    return (sCallbacks->addr [kSetIntervalRoleTimerCallback]) (inIdleMsec);     
}

QTSS_Error  QTSS_RequestGlobalLock()
{
    return (sCallbacks->addr [kRequestGlobalLockCallback])  ();
}

// SYNCH GLOBAL MULTIPLE READERS/SINGLE WRITER ROUTINES

Bool16  QTSS_IsGlobalLocked()
{
    return (Bool16) (sCallbacks->addr [kIsGlobalLockedCallback])  ();
}

QTSS_Error  QTSS_GlobalUnLock()
{
    return (sCallbacks->addr [kUnlockGlobalLock])  ();
}

QTSS_Error  QTSS_LockObject(QTSS_Object inObject)
{
    return (sCallbacks->addr [kLockObjectCallback])  (inObject);
}

QTSS_Error  QTSS_UnlockObject(QTSS_Object inObject)
{
    return (sCallbacks->addr [kUnlockObjectCallback])  (inObject);
}

// AUTHENTICATION AND AUTHORIZATION ROUTINE 认证和授权


QTSS_Error  QTSS_Authenticate(  const char* inAuthUserName, 
                                const char* inAuthResourceLocalPath, 
                                const char* inAuthMoviesDir, 
                                QTSS_ActionFlags inAuthRequestAction, 
                                QTSS_AuthScheme inAuthScheme, 
                                QTSS_RTSPRequestObject ioAuthRequestObject)
{
    return (sCallbacks->addr [kAuthenticateCallback]) (inAuthUserName, inAuthResourceLocalPath, inAuthMoviesDir, inAuthRequestAction, inAuthScheme, ioAuthRequestObject);
}

QTSS_Error	QTSS_Authorize(QTSS_RTSPRequestObject inAuthRequestObject, char** outAuthRealm, Bool16* outAuthUserAllowed)
{
    return (sCallbacks->addr [kAuthorizeCallback]) (inAuthRequestObject, outAuthRealm, outAuthUserAllowed);
}

//STANDARD LIBRARY ROUTINE CALLBACKS

void  QTSS_LockStdLib()
{
   (sCallbacks->addr [kLockStdLibCallback])  ();
}

void  QTSS_UnlockStdLib()
{
    (sCallbacks->addr [kUnlockStdLibCallback])  ();
}

