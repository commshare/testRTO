
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSModuleUtils.cpp
Description: define the Utility routines for modules to use..
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include "QTSSModuleUtils.h"
#include "QTSS_Private.h"

#include "QTAccessFile.h"
#include "OSArrayObjectDeleter.h"
#include "OSMemory.h"
#include "MyAssert.h"

#include "ResizeableStringFormatter.h"
#include "StringFormatter.h"
#include "StrPtrLen.h"
#include "StringParser.h"
#include "SafeStdLib.h"

#include <netinet/in.h>



/* 初始化这些static data members  */
QTSS_TextMessagesObject     QTSSModuleUtils::sMessages = NULL;
QTSS_ServerObject           QTSSModuleUtils::sServer = NULL;
QTSS_StreamRef              QTSSModuleUtils::sErrorLog = NULL;

Bool16                      QTSSModuleUtils::sEnableRTSPErrorMsg = false;/* 设置不发送RTSP Error Message */
QTSS_ErrorVerbosity         QTSSModuleUtils::sMissingPrefVerbosity = qtssMessageVerbosity;/* 设置没设置服务器预设值的错误日志级别, used in QTSServerPrefs::RereadServerPreferences() */

/* used in QTSServer::Initialize(),用入参初始化静态数据成员,此处是为了便于记录error log */
void    QTSSModuleUtils::Initialize(QTSS_TextMessagesObject inMessages,
                                    QTSS_ServerObject inServer,
                                    QTSS_StreamRef inErrorLog)
{
    sMessages = inMessages;
    sServer = inServer;
    sErrorLog = inErrorLog;
}

/* used in DoDescribe() in QTSSFileModule */
/* 从指定文件路径inPath读出文件数据存放在动态创建的缓存outData处,并将该文件的修改时间信息赋给入参outModDate */
QTSS_Error QTSSModuleUtils::ReadEntireFile(char* inPath, StrPtrLen* outData, QTSS_TimeVal inModDate, QTSS_TimeVal* outModDate)
{   
    
    QTSS_Object theFileObject = NULL;
    QTSS_Error theErr = QTSS_NoErr;
    
    outData->Ptr = NULL;
    outData->Len = 0;
    
    do { 
        // Use the QTSS file system API to read the file
		/* QTSS_OpenFileObject回调例程打开指定的文件并返回相应的文件对象.文件对象的属性之一是一个流引用，可以传递给QTSS流回调例程，
		   以便读写文件数据，或者执行其它文件操作。inPath是一个以null结尾的C字符串，表示即将被打开的文件在
		   本地文件系统中的全路径.&theFileObject是一个指针，指向类型为QTSS_Object的值，打开后的文件对应的文件对象(QTSSFile实例)将存放在这里.
		   qtssOpenFileNoFlags = 0,表示没有指定打开标志。*/
        theErr = QTSS_OpenFileObject(inPath, 0, &theFileObject);
        if (theErr != QTSS_NoErr)
            break;
    
		/* 获取文件对象修改时间属性 */
        UInt32 theParamLen = 0;
        QTSS_TimeVal* theModDate = NULL;
        theErr = QTSS_GetValuePtr(theFileObject, qtssFlObjModDate, 0, (void**)&theModDate, &theParamLen);
        Assert(theParamLen == sizeof(QTSS_TimeVal));
        if(theParamLen != sizeof(QTSS_TimeVal))
            break;
		/* 及时更新文件修改时间属性,原来的Default值为NULL */
        if(outModDate != NULL)
            *outModDate = (QTSS_TimeVal)*theModDate;

		/* 假如Default值-1已被修改,一定要保证它被文件的修改时间大 */
        if(inModDate != -1) 
		{   
            // If file hasn't been modified since inModDate, don't have to read the file
            if(*theModDate <= inModDate)
                break;
        }
        
		/* 原来表示文件修改时间的长度,现在重置以便下面获取文件长度属性应用 */
        theParamLen = 0;
        UInt64* theLength = NULL;
		/* 获取打开的文件对象的长度属性 */
        theErr = QTSS_GetValuePtr(theFileObject, qtssFlObjLength, 0, (void**)&theLength, &theParamLen);
        if (theParamLen != sizeof(UInt64))
            break;
        
		/* 确保文件长度不得超过最大限定值,注意对4G的大文件,此处会做修改 */
		if (*theLength > kSInt32_Max)
			break;

        // Allocate memory for the file data
		/* 注意多分配一个字符存放'\0',得到的文件数据是个C-String */
		/* 分配指定长度的缓存来存放打开的文件数据 */
        outData->Ptr = NEW char[ (SInt32) (*theLength + 1) ];
        outData->Len = (SInt32) *theLength;
        outData->Ptr[outData->Len] = 0;
    
        // Read the data
        UInt32 recvLen = 0;
		/* 注意文件对象是个流引用,可以使用Stream Callback routines */
		/* QTSS_Read回调例程从一个流中读取数据，放入缓冲区.theFileObject是一个类型为QTSS_StreamRef的值，
		   指定即将读取数据的流。调用QTSS_OpenFileObject函数可以得到您希望读取的文件的流引用。recvLen表
		   示读取的字节数。 */
		/* 从打开的文件对象中读取全部数据到新建的缓冲区 */
        theErr = QTSS_Read(theFileObject, outData->Ptr, outData->Len, &recvLen);
		/* 若读取文件数据出错,就删去该buffer中的数据,中断退出 */
        if (theErr != QTSS_NoErr)
        {
            outData->Delete();
            break;
        } 
		/* 一定要确保文件数据全部读出 */
        Assert(outData->Len == recvLen);
    
    }while(false);
    
    // Close the file
	/* 读取完文件数据后关闭该文件 */
    if(theFileObject != NULL) {
        theErr = QTSS_CloseFileObject(theFileObject);
    }
    
    return theErr;
}

/* 在服务器支持的TRSP methods的个数的基础上加入入参inNumMethods指定个数的RTSP methods */
void    QTSSModuleUtils::SetupSupportedMethods(QTSS_Object inServer, QTSS_RTSPMethod* inMethodArray, UInt32 inNumMethods)
{
    // Report to the server that this module handles DESCRIBE, SETUP, TEARDOWN, PLAY , and PAUSE
	/* 获取服务器支持的TRSP methods的个数 */
    UInt32 theNumMethods = 0;
    (void)QTSS_GetNumValues(inServer, qtssSvrHandledMethods, &theNumMethods);
    
	/* 循环设置服务器支持的TRSP methods(计数从服务器支持的TRSP methods的个数基础上开始),注意数组inMethodArray的定义已固定的,参见QTSSRTSPProtocol.h */
    for (UInt32 x = 0; x < inNumMethods; x++)
        (void)QTSS_SetValue(inServer, qtssSvrHandledMethods, theNumMethods++, (void*)&inMethodArray[x], sizeof(inMethodArray[x]));
}

/* 获取指定QTSS_AttributeID的属性信息(字符串格式信息),将后两个参数以这个指定格式存入新建的缓存,再以指定写入标志写入Error Log流 */
/* 这个函数以后使用极其频繁!! */
void    QTSSModuleUtils::LogError(  QTSS_ErrorVerbosity inVerbosity,/* 错误日志级别 */
                                    QTSS_AttributeID inTextMessage,/* 错误字符串索引 */
                                    UInt32 /*inErrNumber*/, /* 该参数无用 */
                                    char* inArgument,
                                    char* inArg2)
{
    static char* sEmptyArg = "";
    
	/* 假如TextMessage对象为空,就返回,参见QTSSModuleUtils::Initialize() */
    if (sMessages == NULL)
        return;
        
    // Retrieve the specified text message from the text messages dictionary.
    
	/* 获取一个指定索引值的Text Message属性值的指针 */
    StrPtrLen theMessage;
    (void)QTSS_GetValuePtr(sMessages, inTextMessage, 0, (void**)&theMessage.Ptr, &theMessage.Len);
	/* 若是空属性,就获取空属性"%s%s"的指针 */
    if ((theMessage.Ptr == NULL) || (theMessage.Len == 0))
        (void)QTSS_GetValuePtr(sMessages, qtssMsgNoMessage, 0, (void**)&theMessage.Ptr, &theMessage.Len);

	/* 若还是得到空属性,就退出 */
    if ((theMessage.Ptr == NULL) || (theMessage.Len == 0))
        return;
    
    // qtss_sprintf and ::strlen will crash if inArgument is NULL
	// 设置默认值
    if (inArgument == NULL)
        inArgument = sEmptyArg;
    if (inArg2 == NULL)
        inArg2 = sEmptyArg;
    
    // Create a new string, and put the argument into the new string.
    
	/* 获取错误日志Message的长度 */
    UInt32 theMessageLen = theMessage.Len + ::strlen(inArgument) + ::strlen(inArg2);

	/* 分配指定长度的内存,创建Log string */
    OSCharArrayDeleter theLogString(NEW char[theMessageLen + 1]);
	/* 将后两个参数以指定格式存入新建的缓存 */
    qtss_sprintf(theLogString.GetObject(), theMessage.Ptr, inArgument, inArg2);
	/* 确保数据不溢出 */
    Assert(theMessageLen >= ::strlen(theLogString.GetObject()));
    /* 将上述新建缓存区中的相关信息inMessage写入Error Log流(第一个入参) */
    (void)QTSS_Write(sErrorLog, theLogString.GetObject(), ::strlen(theLogString.GetObject()),NULL, inVerbosity);
}

/* 将指定错误字符串inMessage写入Error Log流,第一个入参是Error Log流 */
void QTSSModuleUtils::LogErrorStr( QTSS_ErrorVerbosity inVerbosity, char* inMessage) 
{  	
	//假如指定错误字符串为空,不写,返回
	if (inMessage == NULL)
		return;  
	(void)QTSS_Write(sErrorLog, inMessage, ::strlen(inMessage), NULL, inVerbosity);
}


/* 动态创建指定长度的缓存,对给定的请求文件路径输出完整的本地文件系统路径C-String的指针起始位置，长度，包括附加的后缀 */
char* QTSSModuleUtils::GetFullPath( QTSS_RTSPRequestObject inRequest,
                                    QTSS_AttributeID whichFileType,/* 请求文件属性ID */
                                    UInt32* outLen,/* 输出文件路径的长度 */
                                    StrPtrLen* suffix)/* RTSP请求的文件可能的后缀 */
{
	/* 确保输出路径的长度非零 */
    Assert(outLen != NULL);
    
	/* 记得最后解锁 */
	(void)QTSS_LockObject(inRequest);
    // Get the proper file path attribute. This may return an error if
    // the file type is qtssFilePathTrunc attr, because there may be no path
    // once its truncated. That's ok. In that case, we just won't append a path.
	/* 获取指定文件类型的RTSP request的文件路径指针 */
    StrPtrLen theFilePath;
    (void)QTSS_GetValuePtr(inRequest, whichFileType, 0, (void**)&theFilePath.Ptr, &theFilePath.Len);
	
	/* acquire root dir of  RTSP requested file.The default value for this parameter is the server's media folder path. 
	   ie, "C:\Program Files\Darwin Streaming Server\Movies" */
    StrPtrLen theRootDir;
    QTSS_Error theErr = QTSS_GetValuePtr(inRequest, qtssRTSPReqRootDir, 0, (void**)&theRootDir.Ptr, &theRootDir.Len);
	Assert(theErr == QTSS_NoErr);


    //trim off extra / characters before concatenating
    // so root/ + /path instead of becoming root//path  is now root/path  as it should be.
    /* 在拼合根目录和文件路径时去除多余的路径分隔符‘/’,注意这样处理的话,/path已变为path */
	if (theRootDir.Len && theRootDir.Ptr[theRootDir.Len -1] == kPathDelimiterChar
	    && theFilePath.Len  && theFilePath.Ptr[0] == kPathDelimiterChar)
	{
	    char *thePathEnd = &(theFilePath.Ptr[theFilePath.Len]);/* 特别记下文件路径的末尾后的第一个字符(不算作文件路径),这是分界符 */
		/* 当文件路径指针还没到达文件路径最末端时,在文件路径开头移动一个字符就停下,及时调整文件路径指针和长度 */
	    while (theFilePath.Ptr != thePathEnd)
	    {
			/* 仅第一次文件路径指针满足这个条件,以后皆不满足 */
	        if (*theFilePath.Ptr != kPathDelimiterChar)
	            break;
	         
			/* 注意theFilePath前面已没有/ */
	        theFilePath.Ptr ++;
	        theFilePath.Len --;
	    }
	}

    //construct a full path out of the root dir path for this request,
    //and the url path.
	/* 注意文件路径长度(可能)减少1 */
    *outLen = theFilePath.Len + theRootDir.Len + 2;
	/* 假如有后缀还要加上后缀长度 */
    if (suffix != NULL)
        *outLen += suffix->Len;
    
	/* 注意你得自己释放内存 */
    char* theFullPath = NEW char[*outLen];
    
    //write all the pieces of the path into this new buffer.
	/* 将所有的路径片段写进刚创建的这块缓存 */
    StringFormatter thePathFormatter(theFullPath, *outLen);
    thePathFormatter.Put(theRootDir);
	/* 注意theFilePath前面已没有/ */
    thePathFormatter.Put(theFilePath);
	/* 假如后缀存在，放进后缀 */
    if (suffix != NULL)
        thePathFormatter.Put(*suffix);
	/* 最后后缀分隔符 */
    thePathFormatter.PutTerminator();

    *outLen = *outLen - 2;
	
	(void)QTSS_UnlockObject(inRequest);
	
    return theFullPath;
}

/* 对入参提供的RTP MetoInfo逐个域进行检查是否合法和used,若是就复制到新建的缓存中,最后再附加到指定RTSP流的指定报头中 */
/* 阅读这个函数务必要搞清楚StringFormatter.h,同时参见RTPMetaInfoPacket::ConstructFieldIDArrayFromHeader() */
QTSS_Error  QTSSModuleUtils::AppendRTPMetaInfoHeader(   QTSS_RTSPRequestObject inRequest,
                                                        StrPtrLen* inRTPMetaInfoHeader,//提供RTP Metal information
                                                        RTPMetaInfoPacket::FieldID* inFieldIDArray) //数组分量为SInt32,提供相应FieldIDArray的信息(最后也被调整):每个field是否compressed?是否not used?
{
    //
    // For formatting the response header
	/* 新建缓存以存放已经合法性检查的格式化的RTPMetaInfoHeader */
    char tempBuffer[128];
    ResizeableStringFormatter theFormatter(tempBuffer, 128);
    
	/* 传递进入参进而来分析该RTPMetaInfoHeader */
    StrPtrLen theHeader(*inRTPMetaInfoHeader);
    
    //
    // For marking which fields were requested by the client
	/* 创建一个新的数组并初始化为0,它的Boolean分量标明了哪个域被client request? */
    Bool16 foundFieldArray[RTPMetaInfoPacket::kNumFields];//6
    ::memset(foundFieldArray, 0, sizeof(Bool16) * RTPMetaInfoPacket::kNumFields);
    
	/* 标记RTPMetaInfoHeader头的末端指针 */
    char* theEndP = theHeader.Ptr + theHeader.Len;
	/* 每个fieldName的实际数值(UInt16) */
    UInt16 fieldNameValue = 0;
    
	/* 逐个检查theHeader中的Field name,合乎要求的复制进缓存theFormatter */
    while (theHeader.Ptr <= (theEndP - sizeof(RTPMetaInfoPacket::FieldName))) //UInt16
    {
		/* 获取2个字节的内容 */
        RTPMetaInfoPacket::FieldName* theFieldName = (RTPMetaInfoPacket::FieldName*)theHeader.Ptr;//UInt16
		/* 获得theHeader.Ptr指向的FieldName的值 */
        ::memcpy (&fieldNameValue, theFieldName, sizeof(UInt16));

		/* 从FieldName的值得到数组inFieldIDArray的FieldIndex */
        RTPMetaInfoPacket::FieldIndex theFieldIndex = RTPMetaInfoPacket::GetFieldIndexForName(ntohs(fieldNameValue));
        
        //
        // This field is not supported (not in the field ID array), so
        // don't put it in the response
		/* 跳过不支持的field或没有used Field ID对应的Field(它们的长度设为3=Field Name(UInt16)+":"),直到遇到符合要求的Field */
        if ((theFieldIndex == RTPMetaInfoPacket::kIllegalField) ||
            (inFieldIDArray[theFieldIndex] == RTPMetaInfoPacket::kFieldNotUsed))
        {
            theHeader.Ptr += 3;
            continue;
        }
        
        //
        // Mark that this field has been requested by the client
		/* 设置创建的数组的分量值 */
        foundFieldArray[theFieldIndex] = true;
        
        //
        // This field is good to go... put it in the response
		/* 将该field name复制进开头创建的内存theFormatter */
        theFormatter.Put(theHeader.Ptr, sizeof(RTPMetaInfoPacket::FieldName));//UInt16
        
		/* 若是compressed Field,就在内存theFormatter中设置该compressed field name的标记 */
        if (inFieldIDArray[theFieldIndex] != RTPMetaInfoPacket::kUncompressed)
        {
            //
            // If the caller wants this field to be compressed (there
            // is an ID associated with the field), put the ID in the response
            theFormatter.PutChar('=');
            theFormatter.Put(inFieldIDArray[theFieldIndex]);//数组分量为SInt32
        }
        
        //
        // Field separator
        theFormatter.PutChar(';');
            
        //
        // Skip onto the next field name in the header
        theHeader.Ptr += 3;
    }

    //
    // Go through the caller's FieldID array, and turn off the fields
    // that were not requested by the client.
	/* 依据foundFieldArray[]的各分量值,将入参inFieldIDArray[]中没用到的Field关闭 */
    for (UInt32 x = 0; x < RTPMetaInfoPacket::kNumFields; x++)
    {
        if (!foundFieldArray[x])
            inFieldIDArray[x] = RTPMetaInfoPacket::kFieldNotUsed;
    }
    
    //
    // No intersection between requested headers and supported headers!
	/* 查看缓存中指针的位置,确保缓存中已经写进数据 */
    if (theFormatter.GetCurrentOffset() == 0)
        return QTSS_ValueNotFound; // Not really the greatest error!
        
    //
    // When appending the header to the response, strip off the last ';'(this is why substract 1 ?).
    // It's not needed.
	/* QTSS_AppendRTSPHeader回调例程将指定报头信息附加到RTSP报头中。在调用QTSS_AppendRTSPHeader函数之后，可以紧接着调用QTSS_SendRTSPHeaders函数来发送整个报头 */
    return QTSS_AppendRTSPHeader(inRequest, qtssXRTPMetaInfoHeader, theFormatter.GetBufPtr(), theFormatter.GetCurrentOffset() - 1);
}


/*************************  注意下面四个函数是关于Send Error Text Message/Response *********************************************/

/* 设置指定RTSP Request的状态码,通知Client将关闭会话,同时向RTSP request stream中写入指定Attribute ID的formatted error text Message给客户端,同时设置QTSS_RTSPRequestAttributes中的相关属性
   RTSP Status Code,ResponseKeepAlive,qtssRTSPReqRespMsg. 该函数总返回QTSS_RequestFailed */
QTSS_Error  QTSSModuleUtils::SendErrorResponse( QTSS_RTSPRequestObject inRequest,/* 代表一个RTSP request stream */
                                                QTSS_RTSPStatusCode inStatusCode,/* 用于设置request stream的RTSP Status Code */
                                                QTSS_AttributeID inTextMessage,/* 指定text Message的Attribute ID */
                                                StrPtrLen* inStringArg)/* 可能的string arg,用来代替指定Attribute ID获得的text Message中的字符串"%s" */
{
    static Bool16 sFalse = false;
    
    //set RTSP headers necessary for this error response message
	/* 用入参设置RTSP Status Code */
	//set status code for access log
    (void)QTSS_SetValue(inRequest, qtssRTSPReqStatusCode, 0, &inStatusCode, sizeof(inStatusCode));
	/* not make the server keep the connection alive(ResponseKeepAlive) */
	// tell the server to end the session
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespKeepAlive, 0, &sFalse, sizeof(sFalse));
	/* 设置存放formatted Error Message的缓存管理类(此处并未分配缓存),进一步设置见下面 */
    StringFormatter theErrorMsgFormatter(NULL, 0);
	/* 准备分配buffer的指针,见下面 */
    char *messageBuffPtr = NULL;
    
	/* 注意这个数据成员的值在程序开头先初始化为false了,可用SetEnableRTSPErrorMsg()设置 */
    if (sEnableRTSPErrorMsg)
    {
        // Retrieve the specified message out of the text messages dictionary.
		/* 获取指定Attribute ID的 text Message */
        StrPtrLen theMessage;
        (void)QTSS_GetValuePtr(sMessages, inTextMessage, 0, (void**)&theMessage.Ptr, &theMessage.Len);

		/* 若得出的text Message为NULL,就用Text Message对象中default "No Message" message传递给client */
        if ((theMessage.Ptr == NULL) || (theMessage.Len == 0))
        {
            // If we couldn't find the specified message, get the default
            // "No Message" message, and return that to the client instead.
            /* 获取default "No Message" message */
            (void)QTSS_GetValuePtr(sMessages, qtssMsgNoMessage, 0, (void**)&theMessage.Ptr, &theMessage.Len);
        }
		/* 确保text Message非空 */
        Assert(theMessage.Ptr != NULL);
        Assert(theMessage.Len > 0);
        
        // Allocate a temporary buffer for the error message, and format the error message into that buffer 
		/* 确定error message在buffer中的长度,至少256字节 */
        UInt32 theMsgLen = 256;
        if (inStringArg != NULL)
            theMsgLen += inStringArg->Len;//加上变量字符串长度
        
		/* 分配指定长度的buffer以存储formatted error message */
        messageBuffPtr = NEW char[theMsgLen];
        messageBuffPtr[0] = 0;
		/* 进一步设置存放formatted Error Message的缓存管理类 */
        theErrorMsgFormatter.Set(messageBuffPtr, theMsgLen);
        
        // Look for a %s in the string, and if one exists, replace it with the
        // argument *inStringArg passed into this function.
        // we can safely assume that message is in fact NULL terminated
		/* 从指定Attribute ID获得的text Message中查找字符"%s",返回它之后的字符串 */
        char* stringLocation = ::strstr(theMessage.Ptr, "%s");
        if (stringLocation != NULL)
        {
            //write first chunk
			/* 将从指定Attribute ID获得的text Message中从起始位置到字符"%s"之前的字符串放进buffer */
            theErrorMsgFormatter.Put(theMessage.Ptr, stringLocation - theMessage.Ptr);
            
			/* 假如入参inStringArg存在,用它代替"%s"并放入buffer */
            if (inStringArg != NULL && inStringArg->Len > 0)
            {
                //write string arg if it exists
                theErrorMsgFormatter.Put(inStringArg->Ptr, inStringArg->Len);
				/* 注意现在stringLocation指向"%s"后的字符串 */
                stringLocation += 2;
            }
            //write last chunk
			/* 现在将"%s"后的字符串也放入buffer */
            theErrorMsgFormatter.Put(stringLocation, (theMessage.Ptr + theMessage.Len) - stringLocation);
        }
        else
			/* 如果没有"%s",直接将从指定Attribute ID获得的text Message放入buffer */
            theErrorMsgFormatter.Put(theMessage);
        
        /* 将上述buffer中的写入长度以指定格式存入buffer */
        char buff[32];
        qtss_sprintf(buff,"%lu",theErrorMsgFormatter.GetBytesWritten());
		/* QTSS_AppendRTSPHeader回调例程将qtssContentLengthHeader报头信息附加到RTSP报头中。在调用QTSS_AppendRTSPHeader函数之后，可以紧接着调用QTSS_SendRTSPHeaders函数来发送整个报头。 */
		/* 向RTSPResponseStream中放入"Content-length: 1209\r\n" */
        (void)QTSS_AppendRTSPHeader(inRequest, qtssContentLengthHeader, buff, ::strlen(buff));
    }
    
    //send the response header. In all situations where errors could happen, we
    //don't really care, cause there's nothing we can do anyway!
	/* QTSS_SendRTSPHeaders回调例程发送一个RTSP报头。当模块调用QTSS_SendRTSPHeaders函数的时候，服务器会使用请求的当前状态码发送一个正确的RTSP状态行。服务器还会发送正确的CSeq报头，会话ID报头，以及连接报头。 */
    //将上述缓存管理theErrorMsgFormatter中的text Message放入RTSPResponseStream中
	(void)QTSS_SendRTSPHeaders(inRequest);

    //
    // Now that we've formatted the message into the temporary buffer,
    // write it out to the request stream and the Client Session object
	/* 将内存中的formatted Error Message写入request RTSP stream */
	/*********** 这是本函数最重要的一行!! *************************/
    (void)QTSS_Write(inRequest, theErrorMsgFormatter.GetBufPtr(), theErrorMsgFormatter.GetBytesWritten(), NULL, 0);

	// A module sending an RTSP error to the client should set this to be a text message describing why the error occurred. This description is useful to add to log files. Once the RTSP response has been sent, this attribute contains the response message.
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespMsg, 0, theErrorMsgFormatter.GetBufPtr(), theErrorMsgFormatter.GetBytesWritten());
    
	/* 释放存放formatted Error Message的temp buffer */
    delete [] messageBuffPtr;
    return QTSS_RequestFailed;
}


/* 设置指定RTSP Request的状态码,设置不保持活跃,向RTSP request stream中写入入参给定的(没有格式化的)error text Message给客户端,同时设置QTSS_RTSPRequestAttributes中的相关属性
   RTSP Status Code,ResponseKeepAlive,qtssRTSPReqRespMsg. 该函数总返回QTSS_RequestFailed  */
QTSS_Error	QTSSModuleUtils::SendErrorResponseWithMessage( QTSS_RTSPRequestObject inRequest,
														QTSS_RTSPStatusCode inStatusCode,
														StrPtrLen* inErrorMessagePtr)
{
    static Bool16 sFalse = false;
    
    //set RTSP headers necessary for this error response message
	//set status code for access log,设置指定RTSP Request的状态码
    (void)QTSS_SetValue(inRequest, qtssRTSPReqStatusCode, 0, &inStatusCode, sizeof(inStatusCode));
	// tell the server to end the session,设置不保持活跃
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespKeepAlive, 0, &sFalse, sizeof(sFalse));
    StrPtrLen theErrorMessage(NULL, 0);
    
    if (sEnableRTSPErrorMsg)
    {   //务必确保入参存在
		Assert(inErrorMessagePtr != NULL);
		//Assert(inErrorMessagePtr->Ptr != NULL);
		//Assert(inErrorMessagePtr->Len != 0);
		/* 用入参inErrorMessagePtr初始化上面声明的theErrorMessage */
		theErrorMessage.Set(inErrorMessagePtr->Ptr, inErrorMessagePtr->Len);
		
		/* 将入参的长度以指定格式存入buff数组 */
        char buff[32];
        qtss_sprintf(buff,"%lu",inErrorMessagePtr->Len);
		/* 将qtssContentLengthHeader报头信息附加到RTSP报头中,类似"Content-length: 1209\r\n" */
        (void)QTSS_AppendRTSPHeader(inRequest, qtssContentLengthHeader, buff, ::strlen(buff));
    }
    
    //send the response header. In all situations where errors could happen, we
    //don't really care, cause there's nothing we can do anyway!
	//将上述缓存管理theErrorMessage中的指定text Message放入RTSPResponseStream中
    (void)QTSS_SendRTSPHeaders(inRequest);

    //
    // Now that we've formatted the message into the temporary buffer,
    // write it out to the request stream and the Client Session object
    (void)QTSS_Write(inRequest, theErrorMessage.Ptr, theErrorMessage.Len, NULL, 0);
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespMsg, 0, theErrorMessage.Ptr, theErrorMessage.Len);
    
    return QTSS_RequestFailed;
}


/* 向RTSP request stream中写入指定格式的HTTP Error Message信息,同时设置QTSS_RTSPRequestAttributes中的相关属性
   RTSP Status Code,ResponseKeepAlive,qtssRTSPReqRespMsg. 该函数总返回QTSS_RequestFailed */
QTSS_Error	QTSSModuleUtils::SendHTTPErrorResponse( QTSS_RTSPRequestObject inRequest,
													QTSS_SessionStatusCode inStatusCode,
                                                    Bool16 inKillSession,/* 关闭Session吗? */
                                                    char *errorMessage)/* 提供的error Message body */
{
    static Bool16 sFalse = false;
    
    //set status code for access log
    (void)QTSS_SetValue(inRequest, qtssRTSPReqStatusCode, 0, &inStatusCode, sizeof(inStatusCode));

    if (inKillSession) // tell the server to end the session
        (void)QTSS_SetValue(inRequest, qtssRTSPReqRespKeepAlive, 0, &sFalse, sizeof(sFalse));

    /********** 注意下面这两个buffer非常重要!! *******************/

    ResizeableStringFormatter theErrorMessage(NULL, 0); //allocates and deletes memory
	/* 用于第四个参数error Message Body,分析见下 */
    ResizeableStringFormatter bodyMessage(NULL,0); //allocates and deletes memory

	/**************************************************************/

	/* 设置每行64字符,最后一个字符为0 */
    char messageLineBuffer[64]; // used for each line
    static const int maxMessageBufferChars = sizeof(messageLineBuffer) -1;//63
    messageLineBuffer[maxMessageBufferChars] = 0; // guarantee NULL termination

    // ToDo: put in a more meaningful http error message for each error. Not required by spec.
    // ToDo: maybe use the HTTP protcol class static error strings.
	/* 可用更有意义的http error message for each "error" */
    char* errorMsg = "error"; 

	/************** 依次获取时间,RTSP real Status Code,RTSP Server Header准备放入theErrorMessage *************/

	/* 当第二个入参inDate不存在时,将local当前时间调整为GMT.再将GMT时间以指定格式存入入参theDate */
	/* 获取时间信息theDate */
    DateBuffer theDate;
    DateTranslator::UpdateDateBuffer(&theDate, 0); // get the current GMT date and time

	/* 获取RTSP real Status Code */
    UInt32 realCode = 0;
    UInt32 len = sizeof(realCode);
    (void) QTSS_GetValue(inRequest, qtssRTSPReqRealStatusCode, 0,  (void*)&realCode,&len);

	/* 获取RTSP Server Header */
    char serverHeaderBuffer[64]; // the qtss Server: header field
    len = sizeof(serverHeaderBuffer) -1; // leave room for terminator
	//Server: header that the server uses to respond to RTSP clients
    (void) QTSS_GetValue(sServer, qtssSvrRTSPServerHeader, 0,  (void*)serverHeaderBuffer,&len);
    serverHeaderBuffer[len] = 0; // terminate.
 
	/**************************** 将上面的信息以指定格式放入theErrorMessage **************************/

	/* 将形如"HTTP/1.1 ** error"存入一行buffer(63个字符长) */
    qtss_snprintf(messageLineBuffer,maxMessageBufferChars, "HTTP/1.1 %lu %s",realCode, errorMsg);
	/* 将指定长度(63)的格式字符串"HTTP/1.1 ** error"存入缓存中,末尾放上"\0" */
    theErrorMessage.Put(messageLineBuffer,::strlen(messageLineBuffer));
    theErrorMessage.PutEOL();


	/* 将获取的server Header信息放入缓存中,末尾放上"\r\n" */
    theErrorMessage.Put(serverHeaderBuffer,::strlen(serverHeaderBuffer));
    theErrorMessage.PutEOL();
 
	/* 将日期按指定格式放入messageLineBuffer,进一步放入buffer,并加上"\r\n" */
    qtss_snprintf(messageLineBuffer,maxMessageBufferChars, "Date: %s",theDate.GetDateBuffer());
    theErrorMessage.Put(messageLineBuffer,::strlen(messageLineBuffer));
    theErrorMessage.PutEOL();
 
	/* 假如入参errorMessage非空 */
    Bool16 addBody =  (errorMessage != NULL && ::strlen(errorMessage) != 0); // body error message so add body headers
    if (addBody) // body error message so add body headers
    {
        // first create the html body: <html><body>\n
        static const StrPtrLen htmlBodyStart("<html><body>\n");
        bodyMessage.Put(htmlBodyStart.Ptr,htmlBodyStart.Len);
 
        //<h1>errorMessage</h1>\n
        static const StrPtrLen hStart("<h1>");
        bodyMessage.Put(hStart.Ptr,hStart.Len);

        bodyMessage.Put(errorMessage,::strlen(errorMessage));

        static const StrPtrLen hTerm("</h1>\n");
        bodyMessage.Put(hTerm.Ptr,hTerm.Len);
 
		// put into </body></html>\n
        static const StrPtrLen htmlBodyTerm("</body></html>\n");
        bodyMessage.Put(htmlBodyTerm.Ptr,htmlBodyTerm.Len);

		/* 注意现在转向了另一个内存theErrorMessage */
        // write body headers: Content-Type: text/html\r\n
        static const StrPtrLen bodyHeaderType("Content-Type: text/html");
        theErrorMessage.Put(bodyHeaderType.Ptr,bodyHeaderType.Len);
        theErrorMessage.PutEOL();

		// put into Content-Length: *****\r\n
        qtss_snprintf(messageLineBuffer,maxMessageBufferChars, "Content-Length: %lu", bodyMessage.GetBytesWritten());
        theErrorMessage.Put(messageLineBuffer,::strlen(messageLineBuffer));        
        theErrorMessage.PutEOL();
    }

	//put into Connection: close\r\n
    static const StrPtrLen headerClose("Connection: close");
    theErrorMessage.Put(headerClose.Ptr,headerClose.Len);
    theErrorMessage.PutEOL();

	//put into \r\n
    theErrorMessage.PutEOL();  // terminate headers with empty line

	/* 现在将bodyMessage中的信息也放进theErrorMessage */
    if (addBody) // add html body
    {
        theErrorMessage.Put(bodyMessage.GetBufPtr(),bodyMessage.GetBytesWritten());
    }

    //
    // Now that we've formatted the message into the temporary buffer,
    // write it out to the request stream and the Client Session object
	/* 将内存中的formatted HTTP Error Message写入request RTSP stream */
	/* 这是本函数最重要的一行!! */
    (void)QTSS_Write(inRequest, theErrorMessage.GetBufPtr(), theErrorMessage.GetBytesWritten(), NULL, 0);
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespMsg, 0, theErrorMessage.GetBufPtr(), theErrorMessage.GetBytesWritten());
    
    return QTSS_RequestFailed;
}

/* 将qtssContentLengthHeader报头信息附加到RTSP报头中并发送整个报头,再将指定的describeData以iovec结构写入到流中 */
void    QTSSModuleUtils::SendDescribeResponse(QTSS_RTSPRequestObject inRequest,
                                                    QTSS_ClientSessionObject inSession,
                                                    iovec* describeData,
                                                    UInt32 inNumVectors,
                                                    UInt32 inTotalLength)
{
    //write content size header
	/* 给出最后一个入参inTotalLength */
    char buf[32];
    qtss_sprintf(buf, "%ld", inTotalLength);
    (void)QTSS_AppendRTSPHeader(inRequest, qtssContentLengthHeader, &buf[0], ::strlen(&buf[0]));

	/* 将一个标准的响应写入到由inRequest参数指定的RTSP流 */
    (void)QTSS_SendStandardRTSPResponse(inRequest, inSession, 0);

        // On solaris, the maximum # of vectors is very low (= 16) so to ensure that we are still able to
        // send the SDP if we have a number greater than the maximum allowed, we coalesce(拼合) the vectors into
        // a single big buffer

	/* 通过iovec结构将数据写入到流中，其方式类似于POSIX的writev调用。 */
    (void)QTSS_WriteV(inRequest, describeData, inNumVectors, inTotalLength, NULL);
}

/* used in QTSSModuleUtils::SendDescribeResponse() */
/* 将指定的向量(结构体)数组的元素首尾拼合起来重新存放在新建内存中,返回内存起始处指针 */
char*   QTSSModuleUtils::CoalesceVectors(iovec* inVec, UInt32 inNumVectors, UInt32 inTotalLength)
{
    if (inTotalLength == 0)
        return NULL;
    
    char* buffer = NEW char[inTotalLength];
    UInt32 bufferOffset = 0;
    
    for (UInt32 index = 0; index < inNumVectors; index++)
    {
        ::memcpy (buffer + bufferOffset, inVec[index].iov_base, inVec[index].iov_len);
        bufferOffset += inVec[index].iov_len;
    }
    
    Assert (bufferOffset == inTotalLength);
    
    return buffer;
}

/*************** 注意下面三个函数是关于Module Object相关属性的 ****************************/

/* 从指定的Module object获取ModulePrefsObject */
QTSS_ModulePrefsObject QTSSModuleUtils::GetModulePrefsObject(QTSS_ModuleObject inModObject)
{
    QTSS_ModulePrefsObject thePrefsObject = NULL;
    UInt32 theLen = sizeof(thePrefsObject);
    QTSS_Error theErr = QTSS_GetValue(inModObject, qtssModPrefs, 0, &thePrefsObject, &theLen);
    Assert(theErr == QTSS_NoErr);
    
    return thePrefsObject;
}

/* 从指定的Module object获取ModuleAttributesObject */
QTSS_Object QTSSModuleUtils::GetModuleAttributesObject(QTSS_ModuleObject inModObject)
{
    QTSS_Object theAttributesObject = NULL;
    UInt32 theLen = sizeof(theAttributesObject);
    QTSS_Error theErr = QTSS_GetValue(inModObject, qtssModAttributes, 0, &theAttributesObject, &theLen);
    Assert(theErr == QTSS_NoErr);
    
    return theAttributesObject;
}

/* 在QTSS_ServerAttributes中的qtssSvrModuleObjects中遍历查找符合入参指定Module Name的条件的ModuleObject,并返回 */
QTSS_ModulePrefsObject QTSSModuleUtils::GetModuleObjectByName(const StrPtrLen& inModuleName)
{
    QTSS_ModuleObject theModule = NULL;
    UInt32 theLen = sizeof(theModule);
    
	/* 在QTSS_ServerAttributes中的qtssSvrModuleObjects中遍历查找符合条件的ModuleObject */
    for (int x = 0; QTSS_GetValue(sServer, qtssSvrModuleObjects, x, &theModule, &theLen) == QTSS_NoErr; x++)
    {
        Assert(theModule != NULL);
        Assert(theLen == sizeof(theModule));
        
		/* 获取该Module的qtssModName属性值theName */
        StrPtrLen theName;
        QTSS_Error theErr = QTSS_GetValuePtr(theModule, qtssModName, 0, (void**)&theName.Ptr, &theName.Len);
        Assert(theErr == QTSS_NoErr);
        
		/* 查找到入参指定Module Name的Module就立即返回 */
        if (inModuleName.Equal(theName))
            return theModule;
            
#if DEBUG
        theModule = NULL;
        theLen = sizeof(theModule);
#endif
    }
    return NULL;
}


/***************************** 注意下面的函数是关于Attribute **********************************************************/

/* 首先检查指定属性的DataType,并返回正确的属性ID,再将属性值拷贝到一个缓冲区中.如果获取属性值出错,就把default属性值(如果有的话)
   放入缓冲区,记录error log,并创建一个新的属性且返回它的属性ID */
void    QTSSModuleUtils::GetAttribute(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType, 
                                                void* ioBuffer, void* inDefaultValue, UInt32 inBufferLen)
{
    //
    // Check to make sure this attribute is the right type. If it's not, this will coerce(强迫,强制)
    // it to be the right type. This also returns the id of the attribute **评论写得好!***
	/* 检查指定属性的DataType,并返回正确的属性ID(DataType不对的要重新创建新的正确的属性) */
    QTSS_AttributeID theID = QTSSModuleUtils::CheckAttributeDataType(inObject, inAttributeName, inType, inDefaultValue, inBufferLen);

    //
    // Get the attribute value.
	/* 将属性值拷贝到一个缓冲区中,注意作为缓冲区长度,inBufferLen的值已被更新! */
    QTSS_Error theErr = QTSS_GetValue(inObject, theID, 0, ioBuffer, &inBufferLen);
    
    //
    // Caller should KNOW how big this attribute is
	/* 务必确保该缓冲区足够大,否则ioBuffer就是空的 */
    Assert(theErr != QTSS_NotEnoughSpace);
    
	/* 对获取属性值出错进行处理 */
    if (theErr != QTSS_NoErr)
    {
        //
        // If we couldn't get the attribute value for whatever reason, just use the
        // default if it was provided.
        ::memcpy(ioBuffer, inDefaultValue, inBufferLen);

        if (inBufferLen > 0)
        {
            //
            // Log an error for this pref only if there was a default value provided.
			/* 记录下预设值qtssServerPrefMissing进error Log,套路类似QTSSModuleUtils::CheckAttributeDataType() */
            char* theValueAsString = NULL;
            theErr = QTSS_ValueToString(inDefaultValue, inBufferLen, inType, &theValueAsString);
            Assert(theErr == QTSS_NoErr);
            OSCharArrayDeleter theValueStr(theValueAsString);
            QTSSModuleUtils::LogError(  sMissingPrefVerbosity, 
                                        qtssServerPrefMissing,
                                        0,
                                        inAttributeName,
                                        theValueStr.GetObject());
        }
        
        //
        // Create an entry for this attribute                           
        QTSSModuleUtils::CreateAttribute(inObject, inAttributeName, inType, inDefaultValue, inBufferLen);
    }
}

/* 首先检查指定属性的DataType,并返回正确的属性ID,再获取指定属性的值，并转换为C字符串格式并返回;若获取属性值出错,就创建一个新的属性,
   记录error log,并返回入参inDefaultValue */
char*   QTSSModuleUtils::GetStringAttribute(QTSS_Object inObject, char* inAttributeName, char* inDefaultValue)
{
	/* 获取入参inDefaultValue长度 */
    UInt32 theDefaultValLen = 0;
    if (inDefaultValue != NULL)
        theDefaultValLen = ::strlen(inDefaultValue);
    
    //
    // Check to make sure this attribute is the right type. If it's not, this will coerce(强迫,强制)
    // it to be the right type
	/* 检查指定属性的DataType,并返回正确的属性ID(DataType不对的要重新创建新的正确的属性) */
    QTSS_AttributeID theID = QTSSModuleUtils::CheckAttributeDataType(inObject, inAttributeName, qtssAttrDataTypeCharArray, inDefaultValue, theDefaultValLen);

    char* theString = NULL;
	/* 获取指定属性的值，并转换为C字符串格式，存储在由&theString参数指定的内存位置上 */
    (void)QTSS_GetValueAsString(inObject, theID, 0, &theString);
    if (theString != NULL)
        return theString;
    
    //
    // If we get here, the attribute must be missing, so create it and log
    // an error.
    
	/* 假如获取属性值出错,就创建一个新的属性 */
    QTSSModuleUtils::CreateAttribute(inObject, inAttributeName, qtssAttrDataTypeCharArray, inDefaultValue, theDefaultValLen);
    
    //
    // Return the default if it was provided. Only log an error if the default value was provided
	/* 假如提供的inDefaultValue长度非零,就记录error log */
    if (theDefaultValLen > 0)
    {
        QTSSModuleUtils::LogError(  sMissingPrefVerbosity,
                                    qtssServerPrefMissing,
                                    0,
                                    inAttributeName,
                                    inDefaultValue);
    }
    
    /* 假如提供的入参inDefaultValue非空,就分配缓存并返回它 */
    if (inDefaultValue != NULL)
    {
        //
        // Whether to return the default value or not from this function is dependent
        // solely on whether the caller passed in a non-NULL pointer or not.
        // This ensures that if the caller wants an empty-string returned as a default
        // value, it can do that.
		/* 是否返回入参inDefaultValue只由该入参是否非空决定 */
        theString = NEW char[theDefaultValLen + 1];
        ::strcpy(theString, inDefaultValue);
        return theString;
    }
    return NULL;
}

/* 与QTSSModuleUtils::GetAttribute()本质上相同,仅少一个参数 */
void    QTSSModuleUtils::GetIOAttribute(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType,
                            void* ioDefaultResultBuffer, UInt32 inBufferLen)
{
	/* 创建缓存存储入参ioDefaultResultBuffer */
    char *defaultBuffPtr = NEW char[inBufferLen];
    ::memcpy(defaultBuffPtr,ioDefaultResultBuffer,inBufferLen);
	/* 这里第四,五个参数的值是相同的 */
    QTSSModuleUtils::GetAttribute(inObject, inAttributeName, inType, ioDefaultResultBuffer, defaultBuffPtr, inBufferLen);
    delete [] defaultBuffPtr;

}
                            

/* 先从入参inAttributeName属性名称获取QTSS_AttrInfoObject对象，再从其QTSS_AttrInfoObjectAttributes中获得属性ID.
   注意inAttributeName属性名称和属性ID都在QTSS_AttrInfoObjectAttributes中 */
QTSS_AttributeID QTSSModuleUtils::GetAttrID(QTSS_Object inObject, char* inAttributeName)
{
    //
    // Get the attribute ID of this attribute.
	/* 通过属性名称得到一个QTSS_AttrInfoObject对象，可以通过该对象取得属性的名称和ID，属性的数据类型，读写属性值的权限，以及获取属性值的操作是否抢占访问安全 */
    QTSS_Object theAttrInfo = NULL;
    QTSS_Error theErr = QTSS_GetAttrInfoByName(inObject, inAttributeName, &theAttrInfo);
    if (theErr != QTSS_NoErr)
        return qtssIllegalAttrID;

	/* 从QTSS_AttrInfoObject对象获得属性ID */
    QTSS_AttributeID theID = qtssIllegalAttrID; 
    UInt32 theLen = sizeof(theID);
    theErr = QTSS_GetValue(theAttrInfo, qtssAttrID, 0, &theID, &theLen);
    Assert(theErr == QTSS_NoErr);

    return theID;
}

/* 通过属性名称得到一个QTSS_AttrInfoObject对象,再从其QTSS_AttrInfoObjectAttributes中获取属性DataType,若与入参inType不同,则记入error Log,删除并再新建该实例属性,返回(可能新建的)属性ID */
QTSS_AttributeID QTSSModuleUtils::CheckAttributeDataType(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType, void* inDefaultValue, UInt32 inBufferLen)
{
    //
    // Get the attribute type of this attribute.

	/* 通过属性名称得到一个QTSS_AttrInfoObject对象 */
    QTSS_Object theAttrInfo = NULL;
    QTSS_Error theErr = QTSS_GetAttrInfoByName(inObject, inAttributeName, &theAttrInfo);
    if (theErr != QTSS_NoErr)
        return qtssIllegalAttrID;

	/* 从QTSS_AttrInfoObject对象获得属性DataType */
    QTSS_AttrDataType theAttributeType = qtssAttrDataTypeUnknown;
    UInt32 theLen = sizeof(theAttributeType);
    theErr = QTSS_GetValue(theAttrInfo, qtssAttrDataType, 0, &theAttributeType, &theLen);
    Assert(theErr == QTSS_NoErr);
    
    /* 从QTSS_AttrInfoObject对象获得属性ID */
    QTSS_AttributeID theID = qtssIllegalAttrID; 
    theLen = sizeof(theID);
    theErr = QTSS_GetValue(theAttrInfo, qtssAttrID, 0, &theID, &theLen);
    Assert(theErr == QTSS_NoErr);

	/* 现在获得的属性DataType若与入参inType不同 */
    if (theAttributeType != inType)
    {
		/* 将一个格式为QTSS_AttrDataType的属性数据类型转换为C字符串格式的值 */
        char* theValueAsString = NULL;
        theErr = QTSS_ValueToString(inDefaultValue, inBufferLen, inType, &theValueAsString);
        Assert(theErr == QTSS_NoErr);

		/* 字符数组删除类对象 */
        OSCharArrayDeleter theValueStr(theValueAsString);
		/* 获取指定QTSS_AttributeID的属性信息(字符串格式信息),将最后两个参数以这个指定格式存入新建的缓存,再以指定写入标志写入Error Log流 */
        QTSSModuleUtils::LogError(  qtssWarningVerbosity,/* 写入标识 */
                                    qtssServerPrefWrongType,/* 字符串格式信息 */
                                    0,
                                    inAttributeName,
                                    theValueStr.GetObject());
        /* 从inObject参数指定的对象中删除由theID参数指定的实例属性 */                            
        theErr = QTSS_RemoveInstanceAttribute( inObject, theID );
        Assert(theErr == QTSS_NoErr);
        return  QTSSModuleUtils::CreateAttribute(inObject, inAttributeName, inType, inDefaultValue, inBufferLen);
    }
    return theID;
}

/* 先向指定的对象实例添加一个实例属性,再获取属性ID,最后用给定的入参inDefaultValue显式设置指定属性的值,返回属性ID */
QTSS_AttributeID QTSSModuleUtils::CreateAttribute(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType, void* inDefaultValue, UInt32 inBufferLen)
{
	/* 向inObject参数指定的对象实例添加一个实例属性。这个回调函数只能在Register角色中调用 */
    QTSS_Error theErr = QTSS_AddInstanceAttribute(inObject, inAttributeName, NULL, inType);
	/* 如果指定名称的属性已经存在,则返回QTSS_AttrNameExists */
    Assert((theErr == QTSS_NoErr) || (theErr == QTSS_AttrNameExists));
    
	/* 先从入参inAttributeName属性名称获取QTSS_AttrInfoObject对象，再从其QTSS_AttrInfoObjectAttributes中获得属性ID.*/
    QTSS_AttributeID theID = QTSSModuleUtils::GetAttrID(inObject, inAttributeName);
    Assert(theID != qtssIllegalAttrID);
        
    //
    // Caller can pass in NULL for inDefaultValue, in which case we don't add the default
	/* 如果入参inDefaultValue非空,我们就要设置它;否则就不用设置它 */
    if (inDefaultValue != NULL)
    {   
		/* 用给定的入参inDefaultValue显式设置指定属性的值 */
        theErr = QTSS_SetValue(inObject, theID, 0, inDefaultValue, inBufferLen);
        Assert(theErr == QTSS_NoErr);
    }
    return theID;
}

/***下面四个函数是获取QTSS_RTSPRequestAttributes中相关属性的***/

/* 利用获取属性的方法,得到RTSP请求的Request Actions Flags */
QTSS_ActionFlags QTSSModuleUtils::GetRequestActions(QTSS_RTSPRequestObject theRTSPRequest)
{
    // Don't touch write requests
    QTSS_ActionFlags action = qtssActionFlagsNoFlags;
    UInt32 len = sizeof(QTSS_ActionFlags);
    QTSS_Error theErr = QTSS_GetValue(theRTSPRequest, qtssRTSPReqAction, 0, (void*)&action, &len);
    Assert(theErr == QTSS_NoErr);
    Assert(len == sizeof(QTSS_ActionFlags));
    return action;
}

/* 利用获取属性的方法,得到RTSP请求的媒体文件的Local path */
//The full local path to the file. This Attribute is first set after the Routing Role has run and before any other role is called. 
char* QTSSModuleUtils::GetLocalPath_Copy(QTSS_RTSPRequestObject theRTSPRequest)
{   char*   pathBuffStr = NULL;
    QTSS_Error theErr = QTSS_GetValueAsString(theRTSPRequest, qtssRTSPReqLocalPath, 0, &pathBuffStr);
    Assert(theErr == QTSS_NoErr);
    return pathBuffStr;
}

/* 利用获取属性的方法,得到RTSP请求的媒体文件的Root Directory */
//Root directory to use for this request. The default value for this parameter is the server's media folder path. Modules may set this attribute from the QTSS_RTSPRoute_Role.
char* QTSSModuleUtils::GetMoviesRootDir_Copy(QTSS_RTSPRequestObject theRTSPRequest)
{   char*   movieRootDirStr = NULL;
    QTSS_Error theErr = QTSS_GetValueAsString(theRTSPRequest,qtssRTSPReqRootDir, 0, &movieRootDirStr);
    Assert(theErr == QTSS_NoErr);
    return movieRootDirStr;
}

/* 利用获取属性的方法,得到RTSP请求的媒体文件的User Profile */
//Object's username is filled in by the server and its password and group memberships filled in by the authentication module.
QTSS_UserProfileObject QTSSModuleUtils::GetUserProfileObject(QTSS_RTSPRequestObject theRTSPRequest)
{   QTSS_UserProfileObject theUserProfile = NULL;
    UInt32 len = sizeof(QTSS_UserProfileObject);
    QTSS_Error theErr = QTSS_GetValue(theRTSPRequest, qtssRTSPReqUserProfile, 0, (void*)&theUserProfile, &len);
    Assert(theErr == QTSS_NoErr);
    return theUserProfile;
}


/******* 下面3个函数是获取QTSS_UserProfileObjectAttributes中相关属性的 *********/

/* 利用获取属性的方法,得到User Name并返回它 */
char *QTSSModuleUtils::GetUserName_Copy(QTSS_UserProfileObject inUserProfile)
{
    char*   username = NULL;    
    (void) QTSS_GetValueAsString(inUserProfile, qtssUserName, 0, &username);
    return username;
}

/* 利用获取属性的方法,得到User Group数组的属性,返回User Group数组 */
char**  QTSSModuleUtils::GetGroupsArray_Copy(QTSS_UserProfileObject inUserProfile, UInt32 *outNumGroupsPtr)
{
    Assert(NULL != outNumGroupsPtr);

    char** outGroupCharPtrArray = NULL;
    *outNumGroupsPtr = 0;
    
	/* 确保第一个参数非空 */
    if (NULL == inUserProfile)
        return NULL;
    
	/* 获取Number of User Group */
    QTSS_Error theErr = QTSS_GetNumValues (inUserProfile,qtssUserGroups, outNumGroupsPtr);
    if (theErr != QTSS_NoErr || *outNumGroupsPtr == 0)
        return NULL;
    
	
	/* 新建指定大小的User Group */
    outGroupCharPtrArray = NEW char*[*outNumGroupsPtr]; // array of char *
    UInt32 len = 0;
    for (UInt32 index = 0; index < *outNumGroupsPtr; index++)
    {   outGroupCharPtrArray[index] = NULL;
	/* 获取Group数组中每个分量(char数组)的属性 */
        QTSS_GetValuePtr(inUserProfile, qtssUserGroups, index,(void **) &outGroupCharPtrArray[index], &len);
    }   

    return outGroupCharPtrArray;
}

/* 逐个获取user group字符串,并查找是否有与入参inGroup相等的?找到后中断退出并返回true,否则返回false.注意第三个参数没用到 */
Bool16 QTSSModuleUtils::UserInGroup(QTSS_UserProfileObject inUserProfile, char* inGroup, UInt32 inGroupLen)
{
	/* 确保入参都非空 */
	if (NULL == inUserProfile || NULL == inGroup  ||  inGroupLen == 0) 
		return false;
	
	/* 获取user name属性 */
	char *userName = NULL;
	UInt32 len = 0;
	QTSS_GetValuePtr(inUserProfile, qtssUserName, 0, (void **)&userName, &len);
	if (len == 0 || userName == NULL || userName[0] == 0) // no user to check
		return false;

	/* 获取user group 组数属性 */
	UInt32 numGroups = 0;
	QTSS_GetNumValues (inUserProfile,qtssUserGroups, &numGroups);
	if (numGroups == 0) // no groups to check
		return false;

	Bool16 result = false;
	char* userGroup = NULL;
	StrPtrLenDel userGroupStr; //deletes pointer in destructor
	
	/* 逐个获取user group字符串,并查找是否有与入参inGroup相等的?找到后中断退出并返回true */
	for (UInt32 index = 0; index < numGroups; index++)
	{  
		userGroup = NULL;
		/* 获取user group字符串 */
		QTSS_GetValueAsString(inUserProfile, qtssUserGroups, index, &userGroup); //allocates string
		/* 清空以备下次使用 */
		userGroupStr.Delete();
		/* 设置为获取的user group字符串 */
		userGroupStr.Set(userGroup);
		/* 假如与入参inGroup相等 */
		if(userGroupStr.Equal(inGroup))
		{	
			result = true;
			break;
		}
	}   

	return result;
	
}


/* 逐一比较入参AttributeID得到的IP Address和入参*inAddressPtr是否相等,相等返回true,否则返回false */
Bool16 QTSSModuleUtils::AddressInList(QTSS_Object inObject, QTSS_AttributeID listID, StrPtrLen *inAddressPtr)
{
    StrPtrLenDel strDeleter;
	/* used in QTSS_GetValueAsString() */
    char*   theAttributeString = NULL; 
	/* 将入参传递进来以便比较,用到IPComponentStr::IPComponentStr()及IPComponentStr::Set() */
    IPComponentStr inAddress(inAddressPtr);
	/* 容纳入参AttributeID得到的IP Address */
    IPComponentStr addressFromList;
    
	/* 合法性检查 */
    if (!inAddress.Valid())
        return false;

	/* 得到指定AttributeID的值个数 */
    UInt32 numValues = 0;
    (void) QTSS_GetNumValues(inObject, listID, &numValues);
    
    for (UInt32 index = 0; index < numValues; index ++)
    { 
		/* 清空以备重新设置 */
        strDeleter.Delete();
		/* 获取指定ID的指定序号的属性 */
        (void) QTSS_GetValueAsString(inObject, listID, index, &theAttributeString);
		/* 暂时存储指定ID的指定序号的属性 */
        strDeleter.Set(theAttributeString);
 
		/* 设置为IPComponentStr类型,以备比较 */
        addressFromList.Set(&strDeleter);
		/* 比较两个IP地址是否相同? */
        if (addressFromList.Equal(&inAddress))
            return true;
    }

    return false;
}

/* used in QTSSFileModule::DoDescribe() */
/* 逐一比较入参AttributeID得到的属性值和入参*inStrPtr是否相等,相等返回true,否则返回false */
Bool16 QTSSModuleUtils::FindStringInAttributeList(QTSS_Object inObject, QTSS_AttributeID listID, StrPtrLen *inStrPtr)
{
    StrPtrLenDel tempString;
    
	/* 确保入参都非空 */
    if (NULL == inStrPtr || NULL == inStrPtr->Ptr || 0 == inStrPtr->Len)
        return false;

	/* 查找客户会话中属性列表中有几个值 */
    UInt32 numValues = 0;
    (void) QTSS_GetNumValues(inObject, listID, &numValues);
    
    for (UInt32 index = 0; index < numValues; index ++)
    { 
		/* 首先清空已有结果以便下次使用 */
        tempString.Delete();
		/* 取出每次的属性值 */
        (void) QTSS_GetValueAsString(inObject, listID, index, &tempString.Ptr);
		/* 重新设置为取出的属性值 */
        tempString.Set(tempString.Ptr);

		/* 如果能查找到字符串 */
        if (inStrPtr->FindString(tempString.Ptr))
            return true;
            
   }

    return false;
}

/* used in QTSSFileModule::DoDescribe() */
/* 首先从RTP Session中获得字符串userAgentStr,再对入参feature分情形讨论,看获得userAgentStr是否在预设值对象的入参feature指定的属性列表中,若都不在,返回false */
Bool16 QTSSModuleUtils::HavePlayerProfile(QTSS_PrefsObject inPrefObjectToCheck, QTSS_StandardRTSP_Params* inParams, UInt32 feature)
{
	/* 获取RTP Session中的属性qtssCliSesFirstUserAgent */
    StrPtrLenDel userAgentStr;    	
    (void)QTSS_GetValueAsString(inParams->inClientSession, qtssCliSesFirstUserAgent, 0, &userAgentStr.Ptr);
    userAgentStr.Set(userAgentStr.Ptr);//找到后会自动删除
    
	/* 对入参分情形(只有两种情形)讨论: */
    switch (feature)
    {
        case QTSSModuleUtils::kRequiresRTPInfoSeqAndTime:
        {     
			//name of player to match against the player's user agent header
            return QTSSModuleUtils::FindStringInAttributeList(inPrefObjectToCheck,  qtssPrefsPlayersReqRTPHeader, &userAgentStr);

        }
        break;
        
        case QTSSModuleUtils::kAdjustBandwidth:
        {
			//name of player to match against the player's user agent header
            return QTSSModuleUtils::FindStringInAttributeList(inPrefObjectToCheck,  qtssPrefsPlayersReqBandAdjust, &userAgentStr);
        }
        break;
    }
    
    return false;
}

/****************** 下面是类IPComponentStr的成员函数定义 *************************/

IPComponentStr IPComponentStr::sLocalIPCompStr("127.0.0.*");

IPComponentStr::IPComponentStr(char *theAddressPtr)
{
    StrPtrLen sourceStr(theAddressPtr);
	/* 实际上是设置fIsValid */
     (void) this->Set(&sourceStr);    
}

IPComponentStr::IPComponentStr(StrPtrLen *sourceStrPtr)
{
	/* 实际上是设置fIsValid */
    (void) this->Set(sourceStrPtr);    
}

/* 对入参从第一个分量开始循环比较是否有与fAddressComponent[3]相同的,有就设置fIsValid为true,退出,函数返回值为true,否则返回false */
Bool16 IPComponentStr::Set(StrPtrLen *theAddressStrPtr)
{
    fIsValid = false;
   
    StringParser IP_Paser(theAddressStrPtr);
	/* 暂设置这个初值,下面会覆盖该值 */
    StrPtrLen *piecePtr = &fAddressComponent[0];

    while (IP_Paser.GetDataRemaining() > 0) 
    {
		/* fStartGet指针吸收字符,直到到达'.',将经过的部分赋给piecePtr(原来的值被刷新) */
        IP_Paser.ConsumeUntil(piecePtr,'.');

		/* 若得到的这个IP component是NULL,就中断循环,返回fIsValid = false */
        if (piecePtr->Len == 0) 
            break;

        /* fStartGet指针再前移一个字符,即跳过'.' */
        IP_Paser.ConsumeLength(NULL, 1);

        /* 当得到的分量与fAddressComponent[3]相同时,设置fIsValid为true,退出,函数返回值为true */
        if (piecePtr == &fAddressComponent[IPComponentStr::kNumComponents -1])
        {
           fIsValid = true;
           break;
        }
        
		/* 移到fAddressComponent[]的下一个分量 */
        piecePtr++;
    };
     
    return fIsValid;
}

/* 逐一循环比较这两个IP address中这4个分量是否相等,若都相等就返回true,否则返回false */
Bool16 IPComponentStr::Equal(IPComponentStr *testAddressPtr)
{
	/* 确保入参非空 */
    if (testAddressPtr == NULL) 
        return false;
    
	/* 确保两个字符串都是合法的 */
    if ( !this->Valid() || !testAddressPtr->Valid() )
        return false;

	/* 循环迭代这两个IP address中这4个分量,逐一比较 */
    for (UInt16 component= 0 ; component < IPComponentStr::kNumComponents ; component ++)
    {
		/* 分别得到这两个IP address中对应的分量 */
        StrPtrLen *allowedPtr = this->GetComponent(component);
        StrPtrLen *testPtr = testAddressPtr->GetComponent(component);
        
		/* 遇到IP分量为*,就继续前移 */
        if ( testPtr->Equal("*") || allowedPtr->Equal("*") )
            continue;
         
		/* 比较对应的IP分量是否相等? */
        if (!testPtr->Equal(*allowedPtr) ) 
            return false; 
    };  
    
    return true;
}


