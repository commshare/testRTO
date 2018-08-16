
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSModuleUtils.h
Description: define the Utility routines for modules to use.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef _QTSS_MODULE_UTILS_H_
#define _QTSS_MODULE_UTILS_H_

#include <stdlib.h>
#include "SafeStdLib.h"
#include "StrPtrLen.h"
#include "QTSS.h"
#include "RTPMetaInfoPacket.h"

class QTSSModuleUtils
{
    public:
		/* used in QTSSModuleUtils::HavePlayerProfile() */
        enum    {  
                    kRequiresRTPInfoSeqAndTime  = 0, 
                    kAdjustBandwidth            = 1 
                };
    
       /* used in QTSServer::Initialize() */
        static void     Initialize( QTSS_TextMessagesObject inMessages,
                                    QTSS_ServerObject inServer,
                                    QTSS_StreamRef inErrorLog);
    
        // Read the complete contents of the file at inPath into the StrPtrLen.
        // This function allocates memory for the file data dynamically.
		/* used in QTSSFileModule::DoDescribe() */
		/* 从指定文件路径inPath读出文件数据存放在动态创建的缓存outData处,并将该文件的修改时间信息赋给入参outModDate */
        static QTSS_Error   ReadEntireFile(char* inPath, StrPtrLen* outData, QTSS_TimeVal inModDate = -1, QTSS_TimeVal* outModDate = NULL);

        // If your module supports RTSP methods, call this function from your QTSS_Initialize
        // role to tell the server what those methods are.
		/* 在服务器支持的RTSP methods的个数的基础上加入入参inNumMethods指定个数的RTSP methods */
        static void     SetupSupportedMethods(  QTSS_Object inServer,
                                                QTSS_RTSPMethod* inMethodArray,
                                                UInt32 inNumMethods);
                                                
        // Using a message out of the text messages dictionary is a common
        // way to log errors to the error log. Here is a function to
        // make that process very easy.
        /* 获取指定QTSS_AttributeID的属性信息(字符串格式信息),将后两个参数以这个指定格式存入新建的缓存,再以指定写入标志写入Error Log流 */
        static void     LogError(   QTSS_ErrorVerbosity inVerbosity,
                                    QTSS_AttributeID inTextMessage,
                                    UInt32 inErrNumber,
                                    char* inArgument = NULL,
                                    char* inArg2 = NULL);
        
		/* 将缓存区中指定错误信息inMessage写入Error Log流,第一个入参是写入flag,有值qtssWriteFlagsIsRTP和qtssWriteFlagsIsRTCP */
        static void   LogErrorStr( QTSS_ErrorVerbosity inVerbosity, char* inMessage);
     
        // This function constructs a C-string of the full path to the file being requested.
        // You may opt to append an optional suffix(附加可能的后缀), or pass in NULL. You are responsible
        // for disposing this memory
		/* 记住自己释放缓存 */
		/* used in QTSSFileModule::DoDescribe() */

		/* 动态创建指定长度的缓存,对给定的请求文件路径输出完整的本地文件系统路径C-String的指针起始位置，长度，包括附加的后缀 */
        static char* GetFullPath(   QTSS_RTSPRequestObject inRequest,
                                    QTSS_AttributeID whichFileType,
                                    UInt32* outLen,
                                    StrPtrLen* suffix = NULL); //suffix 后缀

        //
        // This function does 2 things:**********************************************************
        // 1.   Compares the enabled fields in the field ID array with the fields in the
        //      x-RTP-Meta-Info header. Turns off the fields in the array that aren't in the request.
        //
        // 2.   Appends the x-RTP-Meta-Info header to the response, using the proper
        //      fields from the array, as well as the IDs provided in the array

		/* 对入参提供的RTP MetoInfo逐个域进行检查是否合法和used,若是就复制到新建的缓存中,最后再附加到指定RTSP流的指定报头中 */
		/* 阅读这个函数务必要搞清楚StringFormatter.h,同时参见RTPMetaInfoPacket::ConstructFieldIDArrayFromHeader() */
        static QTSS_Error   AppendRTPMetaInfoHeader( QTSS_RTSPRequestObject inRequest,
                                                        StrPtrLen* inRTPMetaInfoHeader,
                                                        RTPMetaInfoPacket::FieldID* inFieldIDArray);

		/* 注意下面两个函数发送错误的方式的区别:前者附加的字符串入参可以代替指定属性ID的属性中的"/s"来得到formatted 
		   error text Message,后者error text Message是固定的,直接给出了.也就是说,区别在第三/四个参数上 */

        // This function sends an error to the RTSP client. You must provide a
        // status code for the error, and a text message ID to describe the error.
        //
        // It always returns QTSS_RequestFailed.

        static QTSS_Error   SendErrorResponse(  QTSS_RTSPRequestObject inRequest,
                                                        QTSS_RTSPStatusCode inStatusCode,
                                                        QTSS_AttributeID inTextMessage,
                                                        StrPtrLen* inStringArg = NULL);
														
		// This function sends an error to the RTSP client. You don't have to provide
		// a text message ID, but instead you need to provide the error message in a
		// string(直接给出了发送给client的error text Message)
		// 
		// It always returns QTSS_RequestFailed
		static QTSS_Error	SendErrorResponseWithMessage( QTSS_RTSPRequestObject inRequest,
														QTSS_RTSPStatusCode inStatusCode,
														StrPtrLen* inErrorMessageStr);

        // Sends and HTTP 1.1 error message with an error message in HTML if errorMessage != NULL.
        // The session must be flagged by KillSession set to true to kill.
        // Use the QTSS_RTSPStatusCodes for the inStatusCode, for now they are the same as HTTP.
        //
		// It always returns QTSS_RequestFailed
        static QTSS_Error	SendHTTPErrorResponse( QTSS_RTSPRequestObject inRequest,
													QTSS_SessionStatusCode inStatusCode,
                                                    Bool16 inKillSession,
                                                    char *errorMessage);

        //Modules most certainly don't NEED to use this function, but it is awfully handy(相当便利)
        //if they want to take advantage of it. Using the SDP data provided in the iovec,
        //this function sends a standard describe response.
        //NOTE: THE FIRST ENTRY OF THE IOVEC MUST BE EMPTY!!!!
        static void SendDescribeResponse(QTSS_RTSPRequestObject inRequest,
                                                    QTSS_ClientSessionObject inSession,
                                                    iovec* describeData,
                                                    UInt32 inNumVectors,
                                                    UInt32 inTotalLength);

                
        // Called by SendDescribeResponse to coalesce iovec to a buffer
        // Allocates memory - remember to delete it!
        static char* CoalesceVectors(iovec* inVec, UInt32 inNumVectors, UInt32 inTotalLength);
                                                                                                                                                    
        //
        // SEARCH FOR A SPECIFIC MODULE OBJECT                          
        static QTSS_ModulePrefsObject GetModuleObjectByName(const StrPtrLen& inModuleName);
        
        //
        // GET MODULE PREFS OBJECT
        static QTSS_ModulePrefsObject GetModulePrefsObject(QTSS_ModuleObject inModObject);
        
        // GET MODULE ATTRIBUTES OBJECT
        static QTSS_Object GetModuleAttributesObject(QTSS_ModuleObject inModObject);
        
        //
        // GET ATTRIBUTE
        //
        // This function retrieves an attribute 
        // (from any QTSS_Object, including the QTSS_ModulePrefsObject)
        // with the specified name and type
        // out of the specified object.
        //
        // Caller should pass in a buffer for ioBuffer that is large enough
        // to hold the attribute value. inBufferLen should be set to the length
        // of this buffer.
        //
        // Pass in a buffer containing a default value to use for the attribute
        // in the inDefaultValue parameter. If the attribute isn't found, or is
        // of the wrong type, the default value will be copied into ioBuffer.
        // Also, this function adds the default value to object if it is not
        // found or is of the wrong type. If no default value is provided, the
        // attribute is still added but no value is assigned to it.
        //
        // Pass in NULL for the default value or 0 for the default value length if it is not known.
        //
        // This function logs an error if there was a default value provided.
        static void GetAttribute(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType,
                            void* ioBuffer, void* inDefaultValue, UInt32 inBufferLen);
                            
        static void GetIOAttribute(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType,
                            void* ioDefaultResultBuffer, UInt32 inBufferLen);
        //
        // GET STRING ATTRIBUTE
        //
        // Does the same thing as GetAttribute, but does it for string attribute. Returns a newly
        // allocated buffer with the attribute value inside it.
        //
        // Pass in NULL for the default value or an empty string if the default is not known.
        static char* GetStringAttribute(QTSS_Object inObject, char* inAttributeName, char* inDefaultValue);

        //
        // GET ATTR ID
        //
        // Given an attribute in an object, returns its attribute ID
        // or qtssIllegalAttrID if it isn't found.
        static QTSS_AttributeID GetAttrID(QTSS_Object inObject, char* inAttributeName);
        
        //
        //
        //
        /// Get the type of request. Returns qtssActionFlagsNoFlags on failure.
        //  Result is a bitmap of flags
        //

		/***下面四个函数是获取QTSS_RTSPRequestAttributes中相关属性的***/

        static QTSS_ActionFlags GetRequestActions(QTSS_RTSPRequestObject theRTSPRequest);

        static char* GetLocalPath_Copy(QTSS_RTSPRequestObject theRTSPRequest);
        static char* GetMoviesRootDir_Copy(QTSS_RTSPRequestObject theRTSPRequest);
        static QTSS_UserProfileObject GetUserProfileObject(QTSS_RTSPRequestObject theRTSPRequest);
        

		/******* 下面3个函数是获取QTSS_UserProfileObjectAttributes中相关属性的 *********/

        static char*  GetUserName_Copy(QTSS_UserProfileObject inUserProfile);
        static char** GetGroupsArray_Copy(QTSS_UserProfileObject inUserProfile, UInt32 *outNumGroupsPtr);
        static Bool16 UserInGroup(QTSS_UserProfileObject inUserProfile, char* inGroupName, UInt32 inGroupNameLen);

		/* 设置可以向client发送error text Message,used in QTSServerPrefs::RereadServerPreferences() */
        static void SetEnableRTSPErrorMsg(Bool16 enable) {QTSSModuleUtils::sEnableRTSPErrorMsg = enable; }
        
        static QTSS_AttributeID CreateAttribute(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType, void* inDefaultValue, UInt32 inBufferLen);
  
        static Bool16 AddressInList(QTSS_Object inObject, QTSS_AttributeID listID, StrPtrLen *theAddressPtr);
  
        static void SetMisingPrefLogVerbosity(QTSS_ErrorVerbosity verbosityLevel) { QTSSModuleUtils::sMissingPrefVerbosity = verbosityLevel;}
        static QTSS_ErrorVerbosity GetMisingPrefLogVerbosity() { return QTSSModuleUtils::sMissingPrefVerbosity;}
  
        static Bool16 FindStringInAttributeList(QTSS_Object inObject, QTSS_AttributeID listID, StrPtrLen *inStrPtr);

        static Bool16 HavePlayerProfile(QTSS_PrefsObject inPrefObjectToCheck, QTSS_StandardRTSP_Params* inParams, UInt32 feature);
        
    private:
    
        //
        // Used in the implementation of the above functions
        static QTSS_AttributeID CheckAttributeDataType(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType, void* inDefaultValue, UInt32 inBufferLen);    

		/* static data members */

        static QTSS_TextMessagesObject  sMessages;//文本消息类
        static QTSS_ServerObject        sServer;//QTSServer对象,通常只有一个
        static QTSS_StreamRef           sErrorLog;//错误日志流	
        static Bool16                   sEnableRTSPErrorMsg;/* Enable to send RTSP Error Message? */
        static QTSS_ErrorVerbosity      sMissingPrefVerbosity;/* 未设置服务器预设值的错误日志级别 */
};

/* 该类是比较两个IP address之间关系的类 */
class IPComponentStr
{
    public:
    enum { kNumComponents = 4 };
    
	/* 将一个IP address分解成4个StrPtrLen类型的components */
    StrPtrLen   fAddressComponent[kNumComponents];/* 本类IP地址 */
    Bool16      fIsValid;/* 值由IPComponentStr::Set()确定 */
    static IPComponentStr sLocalIPCompStr;/* 配置为"127.0.0.*",主要为了和本类对象作比较 */

	// CONSTRUCTORS

    IPComponentStr() : fIsValid(false) {}/* 注意default config为false */

	/* 下面这两个函数都是根据入参来设置fIsValid的值 */
    IPComponentStr(char *theAddress);
    IPComponentStr(StrPtrLen *sourceStrPtr);

/* 从一个IP address中获取一个component,返回该component的指针 */    
inline  StrPtrLen*  GetComponent(UInt16 which); //see below

        /* 逐一循环比较这两个IP address中这4个分量是否相等,若都相等就返回true,否则返回false */
        Bool16      Equal(IPComponentStr *testAddressPtr);

		/* 对入参从第一个分量开始循环比较是否有与fAddressComponent[3]相同的,有就设置fIsValid为true,退出,函数返回值为true,否则返回false */
        Bool16      Set(StrPtrLen *theAddressStrPtr);

        Bool16      Valid() { return fIsValid; }

		/* 判断sLocalIPCompStr是否和本类对象相等?相等返回true,否则返回false */
inline  Bool16      IsLocal();  //see below

};

/* 判断sLocalIPCompStr是否和本类对象相等?相等返回true,否则返回false */
Bool16  IPComponentStr::IsLocal()
{
    if (this->Equal(&sLocalIPCompStr))
        return true;
    
    return false;
}

/* 从一个IP address中获取一个component,返回该component的指针 */
StrPtrLen* IPComponentStr::GetComponent(UInt16 which) 
{
   if (which < IPComponentStr::kNumComponents) 
        return &fAddressComponent[which]; 
   
   Assert(0);
   return NULL; 
}

#endif //_QTSS_MODULE_UTILS_H_
