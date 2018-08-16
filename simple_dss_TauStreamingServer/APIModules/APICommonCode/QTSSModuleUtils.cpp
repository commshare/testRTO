
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



/* ��ʼ����Щstatic data members  */
QTSS_TextMessagesObject     QTSSModuleUtils::sMessages = NULL;
QTSS_ServerObject           QTSSModuleUtils::sServer = NULL;
QTSS_StreamRef              QTSSModuleUtils::sErrorLog = NULL;

Bool16                      QTSSModuleUtils::sEnableRTSPErrorMsg = false;/* ���ò�����RTSP Error Message */
QTSS_ErrorVerbosity         QTSSModuleUtils::sMissingPrefVerbosity = qtssMessageVerbosity;/* ����û���÷�����Ԥ��ֵ�Ĵ�����־����, used in QTSServerPrefs::RereadServerPreferences() */

/* used in QTSServer::Initialize(),����γ�ʼ����̬���ݳ�Ա,�˴���Ϊ�˱��ڼ�¼error log */
void    QTSSModuleUtils::Initialize(QTSS_TextMessagesObject inMessages,
                                    QTSS_ServerObject inServer,
                                    QTSS_StreamRef inErrorLog)
{
    sMessages = inMessages;
    sServer = inServer;
    sErrorLog = inErrorLog;
}

/* used in DoDescribe() in QTSSFileModule */
/* ��ָ���ļ�·��inPath�����ļ����ݴ���ڶ�̬�����Ļ���outData��,�������ļ����޸�ʱ����Ϣ�������outModDate */
QTSS_Error QTSSModuleUtils::ReadEntireFile(char* inPath, StrPtrLen* outData, QTSS_TimeVal inModDate, QTSS_TimeVal* outModDate)
{   
    
    QTSS_Object theFileObject = NULL;
    QTSS_Error theErr = QTSS_NoErr;
    
    outData->Ptr = NULL;
    outData->Len = 0;
    
    do { 
        // Use the QTSS file system API to read the file
		/* QTSS_OpenFileObject�ص����̴�ָ�����ļ���������Ӧ���ļ�����.�ļ����������֮һ��һ�������ã����Դ��ݸ�QTSS���ص����̣�
		   �Ա��д�ļ����ݣ�����ִ�������ļ�������inPath��һ����null��β��C�ַ�������ʾ�������򿪵��ļ���
		   �����ļ�ϵͳ�е�ȫ·��.&theFileObject��һ��ָ�룬ָ������ΪQTSS_Object��ֵ���򿪺���ļ���Ӧ���ļ�����(QTSSFileʵ��)�����������.
		   qtssOpenFileNoFlags = 0,��ʾû��ָ���򿪱�־��*/
        theErr = QTSS_OpenFileObject(inPath, 0, &theFileObject);
        if (theErr != QTSS_NoErr)
            break;
    
		/* ��ȡ�ļ������޸�ʱ������ */
        UInt32 theParamLen = 0;
        QTSS_TimeVal* theModDate = NULL;
        theErr = QTSS_GetValuePtr(theFileObject, qtssFlObjModDate, 0, (void**)&theModDate, &theParamLen);
        Assert(theParamLen == sizeof(QTSS_TimeVal));
        if(theParamLen != sizeof(QTSS_TimeVal))
            break;
		/* ��ʱ�����ļ��޸�ʱ������,ԭ����DefaultֵΪNULL */
        if(outModDate != NULL)
            *outModDate = (QTSS_TimeVal)*theModDate;

		/* ����Defaultֵ-1�ѱ��޸�,һ��Ҫ��֤�����ļ����޸�ʱ��� */
        if(inModDate != -1) 
		{   
            // If file hasn't been modified since inModDate, don't have to read the file
            if(*theModDate <= inModDate)
                break;
        }
        
		/* ԭ����ʾ�ļ��޸�ʱ��ĳ���,���������Ա������ȡ�ļ���������Ӧ�� */
        theParamLen = 0;
        UInt64* theLength = NULL;
		/* ��ȡ�򿪵��ļ�����ĳ������� */
        theErr = QTSS_GetValuePtr(theFileObject, qtssFlObjLength, 0, (void**)&theLength, &theParamLen);
        if (theParamLen != sizeof(UInt64))
            break;
        
		/* ȷ���ļ����Ȳ��ó�������޶�ֵ,ע���4G�Ĵ��ļ�,�˴������޸� */
		if (*theLength > kSInt32_Max)
			break;

        // Allocate memory for the file data
		/* ע������һ���ַ����'\0',�õ����ļ������Ǹ�C-String */
		/* ����ָ�����ȵĻ�������Ŵ򿪵��ļ����� */
        outData->Ptr = NEW char[ (SInt32) (*theLength + 1) ];
        outData->Len = (SInt32) *theLength;
        outData->Ptr[outData->Len] = 0;
    
        // Read the data
        UInt32 recvLen = 0;
		/* ע���ļ������Ǹ�������,����ʹ��Stream Callback routines */
		/* QTSS_Read�ص����̴�һ�����ж�ȡ���ݣ����뻺����.theFileObject��һ������ΪQTSS_StreamRef��ֵ��
		   ָ��������ȡ���ݵ���������QTSS_OpenFileObject�������Եõ���ϣ����ȡ���ļ��������á�recvLen��
		   ʾ��ȡ���ֽ����� */
		/* �Ӵ򿪵��ļ������ж�ȡȫ�����ݵ��½��Ļ����� */
        theErr = QTSS_Read(theFileObject, outData->Ptr, outData->Len, &recvLen);
		/* ����ȡ�ļ����ݳ���,��ɾȥ��buffer�е�����,�ж��˳� */
        if (theErr != QTSS_NoErr)
        {
            outData->Delete();
            break;
        } 
		/* һ��Ҫȷ���ļ�����ȫ������ */
        Assert(outData->Len == recvLen);
    
    }while(false);
    
    // Close the file
	/* ��ȡ���ļ����ݺ�رո��ļ� */
    if(theFileObject != NULL) {
        theErr = QTSS_CloseFileObject(theFileObject);
    }
    
    return theErr;
}

/* �ڷ�����֧�ֵ�TRSP methods�ĸ����Ļ����ϼ������inNumMethodsָ��������RTSP methods */
void    QTSSModuleUtils::SetupSupportedMethods(QTSS_Object inServer, QTSS_RTSPMethod* inMethodArray, UInt32 inNumMethods)
{
    // Report to the server that this module handles DESCRIBE, SETUP, TEARDOWN, PLAY , and PAUSE
	/* ��ȡ������֧�ֵ�TRSP methods�ĸ��� */
    UInt32 theNumMethods = 0;
    (void)QTSS_GetNumValues(inServer, qtssSvrHandledMethods, &theNumMethods);
    
	/* ѭ�����÷�����֧�ֵ�TRSP methods(�����ӷ�����֧�ֵ�TRSP methods�ĸ��������Ͽ�ʼ),ע������inMethodArray�Ķ����ѹ̶���,�μ�QTSSRTSPProtocol.h */
    for (UInt32 x = 0; x < inNumMethods; x++)
        (void)QTSS_SetValue(inServer, qtssSvrHandledMethods, theNumMethods++, (void*)&inMethodArray[x], sizeof(inMethodArray[x]));
}

/* ��ȡָ��QTSS_AttributeID��������Ϣ(�ַ�����ʽ��Ϣ),�����������������ָ����ʽ�����½��Ļ���,����ָ��д���־д��Error Log�� */
/* ��������Ժ�ʹ�ü���Ƶ��!! */
void    QTSSModuleUtils::LogError(  QTSS_ErrorVerbosity inVerbosity,/* ������־���� */
                                    QTSS_AttributeID inTextMessage,/* �����ַ������� */
                                    UInt32 /*inErrNumber*/, /* �ò������� */
                                    char* inArgument,
                                    char* inArg2)
{
    static char* sEmptyArg = "";
    
	/* ����TextMessage����Ϊ��,�ͷ���,�μ�QTSSModuleUtils::Initialize() */
    if (sMessages == NULL)
        return;
        
    // Retrieve the specified text message from the text messages dictionary.
    
	/* ��ȡһ��ָ������ֵ��Text Message����ֵ��ָ�� */
    StrPtrLen theMessage;
    (void)QTSS_GetValuePtr(sMessages, inTextMessage, 0, (void**)&theMessage.Ptr, &theMessage.Len);
	/* ���ǿ�����,�ͻ�ȡ������"%s%s"��ָ�� */
    if ((theMessage.Ptr == NULL) || (theMessage.Len == 0))
        (void)QTSS_GetValuePtr(sMessages, qtssMsgNoMessage, 0, (void**)&theMessage.Ptr, &theMessage.Len);

	/* �����ǵõ�������,���˳� */
    if ((theMessage.Ptr == NULL) || (theMessage.Len == 0))
        return;
    
    // qtss_sprintf and ::strlen will crash if inArgument is NULL
	// ����Ĭ��ֵ
    if (inArgument == NULL)
        inArgument = sEmptyArg;
    if (inArg2 == NULL)
        inArg2 = sEmptyArg;
    
    // Create a new string, and put the argument into the new string.
    
	/* ��ȡ������־Message�ĳ��� */
    UInt32 theMessageLen = theMessage.Len + ::strlen(inArgument) + ::strlen(inArg2);

	/* ����ָ�����ȵ��ڴ�,����Log string */
    OSCharArrayDeleter theLogString(NEW char[theMessageLen + 1]);
	/* ��������������ָ����ʽ�����½��Ļ��� */
    qtss_sprintf(theLogString.GetObject(), theMessage.Ptr, inArgument, inArg2);
	/* ȷ�����ݲ���� */
    Assert(theMessageLen >= ::strlen(theLogString.GetObject()));
    /* �������½��������е������ϢinMessageд��Error Log��(��һ�����) */
    (void)QTSS_Write(sErrorLog, theLogString.GetObject(), ::strlen(theLogString.GetObject()),NULL, inVerbosity);
}

/* ��ָ�������ַ���inMessageд��Error Log��,��һ�������Error Log�� */
void QTSSModuleUtils::LogErrorStr( QTSS_ErrorVerbosity inVerbosity, char* inMessage) 
{  	
	//����ָ�������ַ���Ϊ��,��д,����
	if (inMessage == NULL)
		return;  
	(void)QTSS_Write(sErrorLog, inMessage, ::strlen(inMessage), NULL, inVerbosity);
}


/* ��̬����ָ�����ȵĻ���,�Ը����������ļ�·����������ı����ļ�ϵͳ·��C-String��ָ����ʼλ�ã����ȣ��������ӵĺ�׺ */
char* QTSSModuleUtils::GetFullPath( QTSS_RTSPRequestObject inRequest,
                                    QTSS_AttributeID whichFileType,/* �����ļ�����ID */
                                    UInt32* outLen,/* ����ļ�·���ĳ��� */
                                    StrPtrLen* suffix)/* RTSP������ļ����ܵĺ�׺ */
{
	/* ȷ�����·���ĳ��ȷ��� */
    Assert(outLen != NULL);
    
	/* �ǵ������� */
	(void)QTSS_LockObject(inRequest);
    // Get the proper file path attribute. This may return an error if
    // the file type is qtssFilePathTrunc attr, because there may be no path
    // once its truncated. That's ok. In that case, we just won't append a path.
	/* ��ȡָ���ļ����͵�RTSP request���ļ�·��ָ�� */
    StrPtrLen theFilePath;
    (void)QTSS_GetValuePtr(inRequest, whichFileType, 0, (void**)&theFilePath.Ptr, &theFilePath.Len);
	
	/* acquire root dir of  RTSP requested file.The default value for this parameter is the server's media folder path. 
	   ie, "C:\Program Files\Darwin Streaming Server\Movies" */
    StrPtrLen theRootDir;
    QTSS_Error theErr = QTSS_GetValuePtr(inRequest, qtssRTSPReqRootDir, 0, (void**)&theRootDir.Ptr, &theRootDir.Len);
	Assert(theErr == QTSS_NoErr);


    //trim off extra / characters before concatenating
    // so root/ + /path instead of becoming root//path  is now root/path  as it should be.
    /* ��ƴ�ϸ�Ŀ¼���ļ�·��ʱȥ�������·���ָ�����/��,ע����������Ļ�,/path�ѱ�Ϊpath */
	if (theRootDir.Len && theRootDir.Ptr[theRootDir.Len -1] == kPathDelimiterChar
	    && theFilePath.Len  && theFilePath.Ptr[0] == kPathDelimiterChar)
	{
	    char *thePathEnd = &(theFilePath.Ptr[theFilePath.Len]);/* �ر�����ļ�·����ĩβ��ĵ�һ���ַ�(�������ļ�·��),���Ƿֽ�� */
		/* ���ļ�·��ָ�뻹û�����ļ�·����ĩ��ʱ,���ļ�·����ͷ�ƶ�һ���ַ���ͣ��,��ʱ�����ļ�·��ָ��ͳ��� */
	    while (theFilePath.Ptr != thePathEnd)
	    {
			/* ����һ���ļ�·��ָ�������������,�Ժ�Բ����� */
	        if (*theFilePath.Ptr != kPathDelimiterChar)
	            break;
	         
			/* ע��theFilePathǰ����û��/ */
	        theFilePath.Ptr ++;
	        theFilePath.Len --;
	    }
	}

    //construct a full path out of the root dir path for this request,
    //and the url path.
	/* ע���ļ�·������(����)����1 */
    *outLen = theFilePath.Len + theRootDir.Len + 2;
	/* �����к�׺��Ҫ���Ϻ�׺���� */
    if (suffix != NULL)
        *outLen += suffix->Len;
    
	/* ע������Լ��ͷ��ڴ� */
    char* theFullPath = NEW char[*outLen];
    
    //write all the pieces of the path into this new buffer.
	/* �����е�·��Ƭ��д���մ�������黺�� */
    StringFormatter thePathFormatter(theFullPath, *outLen);
    thePathFormatter.Put(theRootDir);
	/* ע��theFilePathǰ����û��/ */
    thePathFormatter.Put(theFilePath);
	/* �����׺���ڣ��Ž���׺ */
    if (suffix != NULL)
        thePathFormatter.Put(*suffix);
	/* ����׺�ָ��� */
    thePathFormatter.PutTerminator();

    *outLen = *outLen - 2;
	
	(void)QTSS_UnlockObject(inRequest);
	
    return theFullPath;
}

/* ������ṩ��RTP MetoInfo�������м���Ƿ�Ϸ���used,���Ǿ͸��Ƶ��½��Ļ�����,����ٸ��ӵ�ָ��RTSP����ָ����ͷ�� */
/* �Ķ�����������Ҫ�����StringFormatter.h,ͬʱ�μ�RTPMetaInfoPacket::ConstructFieldIDArrayFromHeader() */
QTSS_Error  QTSSModuleUtils::AppendRTPMetaInfoHeader(   QTSS_RTSPRequestObject inRequest,
                                                        StrPtrLen* inRTPMetaInfoHeader,//�ṩRTP Metal information
                                                        RTPMetaInfoPacket::FieldID* inFieldIDArray) //�������ΪSInt32,�ṩ��ӦFieldIDArray����Ϣ(���Ҳ������):ÿ��field�Ƿ�compressed?�Ƿ�not used?
{
    //
    // For formatting the response header
	/* �½������Դ���Ѿ��Ϸ��Լ��ĸ�ʽ����RTPMetaInfoHeader */
    char tempBuffer[128];
    ResizeableStringFormatter theFormatter(tempBuffer, 128);
    
	/* ���ݽ���ν�����������RTPMetaInfoHeader */
    StrPtrLen theHeader(*inRTPMetaInfoHeader);
    
    //
    // For marking which fields were requested by the client
	/* ����һ���µ����鲢��ʼ��Ϊ0,����Boolean�����������ĸ���client request? */
    Bool16 foundFieldArray[RTPMetaInfoPacket::kNumFields];//6
    ::memset(foundFieldArray, 0, sizeof(Bool16) * RTPMetaInfoPacket::kNumFields);
    
	/* ���RTPMetaInfoHeaderͷ��ĩ��ָ�� */
    char* theEndP = theHeader.Ptr + theHeader.Len;
	/* ÿ��fieldName��ʵ����ֵ(UInt16) */
    UInt16 fieldNameValue = 0;
    
	/* ������theHeader�е�Field name,�Ϻ�Ҫ��ĸ��ƽ�����theFormatter */
    while (theHeader.Ptr <= (theEndP - sizeof(RTPMetaInfoPacket::FieldName))) //UInt16
    {
		/* ��ȡ2���ֽڵ����� */
        RTPMetaInfoPacket::FieldName* theFieldName = (RTPMetaInfoPacket::FieldName*)theHeader.Ptr;//UInt16
		/* ���theHeader.Ptrָ���FieldName��ֵ */
        ::memcpy (&fieldNameValue, theFieldName, sizeof(UInt16));

		/* ��FieldName��ֵ�õ�����inFieldIDArray��FieldIndex */
        RTPMetaInfoPacket::FieldIndex theFieldIndex = RTPMetaInfoPacket::GetFieldIndexForName(ntohs(fieldNameValue));
        
        //
        // This field is not supported (not in the field ID array), so
        // don't put it in the response
		/* ������֧�ֵ�field��û��used Field ID��Ӧ��Field(���ǵĳ�����Ϊ3=Field Name(UInt16)+":"),ֱ����������Ҫ���Field */
        if ((theFieldIndex == RTPMetaInfoPacket::kIllegalField) ||
            (inFieldIDArray[theFieldIndex] == RTPMetaInfoPacket::kFieldNotUsed))
        {
            theHeader.Ptr += 3;
            continue;
        }
        
        //
        // Mark that this field has been requested by the client
		/* ���ô���������ķ���ֵ */
        foundFieldArray[theFieldIndex] = true;
        
        //
        // This field is good to go... put it in the response
		/* ����field name���ƽ���ͷ�������ڴ�theFormatter */
        theFormatter.Put(theHeader.Ptr, sizeof(RTPMetaInfoPacket::FieldName));//UInt16
        
		/* ����compressed Field,�����ڴ�theFormatter�����ø�compressed field name�ı�� */
        if (inFieldIDArray[theFieldIndex] != RTPMetaInfoPacket::kUncompressed)
        {
            //
            // If the caller wants this field to be compressed (there
            // is an ID associated with the field), put the ID in the response
            theFormatter.PutChar('=');
            theFormatter.Put(inFieldIDArray[theFieldIndex]);//�������ΪSInt32
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
	/* ����foundFieldArray[]�ĸ�����ֵ,�����inFieldIDArray[]��û�õ���Field�ر� */
    for (UInt32 x = 0; x < RTPMetaInfoPacket::kNumFields; x++)
    {
        if (!foundFieldArray[x])
            inFieldIDArray[x] = RTPMetaInfoPacket::kFieldNotUsed;
    }
    
    //
    // No intersection between requested headers and supported headers!
	/* �鿴������ָ���λ��,ȷ���������Ѿ�д������ */
    if (theFormatter.GetCurrentOffset() == 0)
        return QTSS_ValueNotFound; // Not really the greatest error!
        
    //
    // When appending the header to the response, strip off the last ';'(this is why substract 1 ?).
    // It's not needed.
	/* QTSS_AppendRTSPHeader�ص����̽�ָ����ͷ��Ϣ���ӵ�RTSP��ͷ�С��ڵ���QTSS_AppendRTSPHeader����֮�󣬿��Խ����ŵ���QTSS_SendRTSPHeaders����������������ͷ */
    return QTSS_AppendRTSPHeader(inRequest, qtssXRTPMetaInfoHeader, theFormatter.GetBufPtr(), theFormatter.GetCurrentOffset() - 1);
}


/*************************  ע�������ĸ������ǹ���Send Error Text Message/Response *********************************************/

/* ����ָ��RTSP Request��״̬��,֪ͨClient���رջỰ,ͬʱ��RTSP request stream��д��ָ��Attribute ID��formatted error text Message���ͻ���,ͬʱ����QTSS_RTSPRequestAttributes�е��������
   RTSP Status Code,ResponseKeepAlive,qtssRTSPReqRespMsg. �ú����ܷ���QTSS_RequestFailed */
QTSS_Error  QTSSModuleUtils::SendErrorResponse( QTSS_RTSPRequestObject inRequest,/* ����һ��RTSP request stream */
                                                QTSS_RTSPStatusCode inStatusCode,/* ��������request stream��RTSP Status Code */
                                                QTSS_AttributeID inTextMessage,/* ָ��text Message��Attribute ID */
                                                StrPtrLen* inStringArg)/* ���ܵ�string arg,��������ָ��Attribute ID��õ�text Message�е��ַ���"%s" */
{
    static Bool16 sFalse = false;
    
    //set RTSP headers necessary for this error response message
	/* ���������RTSP Status Code */
	//set status code for access log
    (void)QTSS_SetValue(inRequest, qtssRTSPReqStatusCode, 0, &inStatusCode, sizeof(inStatusCode));
	/* not make the server keep the connection alive(ResponseKeepAlive) */
	// tell the server to end the session
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespKeepAlive, 0, &sFalse, sizeof(sFalse));
	/* ���ô��formatted Error Message�Ļ��������(�˴���δ���仺��),��һ�����ü����� */
    StringFormatter theErrorMsgFormatter(NULL, 0);
	/* ׼������buffer��ָ��,������ */
    char *messageBuffPtr = NULL;
    
	/* ע��������ݳ�Ա��ֵ�ڳ���ͷ�ȳ�ʼ��Ϊfalse��,����SetEnableRTSPErrorMsg()���� */
    if (sEnableRTSPErrorMsg)
    {
        // Retrieve the specified message out of the text messages dictionary.
		/* ��ȡָ��Attribute ID�� text Message */
        StrPtrLen theMessage;
        (void)QTSS_GetValuePtr(sMessages, inTextMessage, 0, (void**)&theMessage.Ptr, &theMessage.Len);

		/* ���ó���text MessageΪNULL,����Text Message������default "No Message" message���ݸ�client */
        if ((theMessage.Ptr == NULL) || (theMessage.Len == 0))
        {
            // If we couldn't find the specified message, get the default
            // "No Message" message, and return that to the client instead.
            /* ��ȡdefault "No Message" message */
            (void)QTSS_GetValuePtr(sMessages, qtssMsgNoMessage, 0, (void**)&theMessage.Ptr, &theMessage.Len);
        }
		/* ȷ��text Message�ǿ� */
        Assert(theMessage.Ptr != NULL);
        Assert(theMessage.Len > 0);
        
        // Allocate a temporary buffer for the error message, and format the error message into that buffer 
		/* ȷ��error message��buffer�еĳ���,����256�ֽ� */
        UInt32 theMsgLen = 256;
        if (inStringArg != NULL)
            theMsgLen += inStringArg->Len;//���ϱ����ַ�������
        
		/* ����ָ�����ȵ�buffer�Դ洢formatted error message */
        messageBuffPtr = NEW char[theMsgLen];
        messageBuffPtr[0] = 0;
		/* ��һ�����ô��formatted Error Message�Ļ�������� */
        theErrorMsgFormatter.Set(messageBuffPtr, theMsgLen);
        
        // Look for a %s in the string, and if one exists, replace it with the
        // argument *inStringArg passed into this function.
        // we can safely assume that message is in fact NULL terminated
		/* ��ָ��Attribute ID��õ�text Message�в����ַ�"%s",������֮����ַ��� */
        char* stringLocation = ::strstr(theMessage.Ptr, "%s");
        if (stringLocation != NULL)
        {
            //write first chunk
			/* ����ָ��Attribute ID��õ�text Message�д���ʼλ�õ��ַ�"%s"֮ǰ���ַ����Ž�buffer */
            theErrorMsgFormatter.Put(theMessage.Ptr, stringLocation - theMessage.Ptr);
            
			/* �������inStringArg����,��������"%s"������buffer */
            if (inStringArg != NULL && inStringArg->Len > 0)
            {
                //write string arg if it exists
                theErrorMsgFormatter.Put(inStringArg->Ptr, inStringArg->Len);
				/* ע������stringLocationָ��"%s"����ַ��� */
                stringLocation += 2;
            }
            //write last chunk
			/* ���ڽ�"%s"����ַ���Ҳ����buffer */
            theErrorMsgFormatter.Put(stringLocation, (theMessage.Ptr + theMessage.Len) - stringLocation);
        }
        else
			/* ���û��"%s",ֱ�ӽ���ָ��Attribute ID��õ�text Message����buffer */
            theErrorMsgFormatter.Put(theMessage);
        
        /* ������buffer�е�д�볤����ָ����ʽ����buffer */
        char buff[32];
        qtss_sprintf(buff,"%lu",theErrorMsgFormatter.GetBytesWritten());
		/* QTSS_AppendRTSPHeader�ص����̽�qtssContentLengthHeader��ͷ��Ϣ���ӵ�RTSP��ͷ�С��ڵ���QTSS_AppendRTSPHeader����֮�󣬿��Խ����ŵ���QTSS_SendRTSPHeaders����������������ͷ�� */
		/* ��RTSPResponseStream�з���"Content-length: 1209\r\n" */
        (void)QTSS_AppendRTSPHeader(inRequest, qtssContentLengthHeader, buff, ::strlen(buff));
    }
    
    //send the response header. In all situations where errors could happen, we
    //don't really care, cause there's nothing we can do anyway!
	/* QTSS_SendRTSPHeaders�ص����̷���һ��RTSP��ͷ����ģ�����QTSS_SendRTSPHeaders������ʱ�򣬷�������ʹ������ĵ�ǰ״̬�뷢��һ����ȷ��RTSP״̬�С����������ᷢ����ȷ��CSeq��ͷ���ỰID��ͷ���Լ����ӱ�ͷ�� */
    //�������������theErrorMsgFormatter�е�text Message����RTSPResponseStream��
	(void)QTSS_SendRTSPHeaders(inRequest);

    //
    // Now that we've formatted the message into the temporary buffer,
    // write it out to the request stream and the Client Session object
	/* ���ڴ��е�formatted Error Messageд��request RTSP stream */
	/*********** ���Ǳ���������Ҫ��һ��!! *************************/
    (void)QTSS_Write(inRequest, theErrorMsgFormatter.GetBufPtr(), theErrorMsgFormatter.GetBytesWritten(), NULL, 0);

	// A module sending an RTSP error to the client should set this to be a text message describing why the error occurred. This description is useful to add to log files. Once the RTSP response has been sent, this attribute contains the response message.
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespMsg, 0, theErrorMsgFormatter.GetBufPtr(), theErrorMsgFormatter.GetBytesWritten());
    
	/* �ͷŴ��formatted Error Message��temp buffer */
    delete [] messageBuffPtr;
    return QTSS_RequestFailed;
}


/* ����ָ��RTSP Request��״̬��,���ò����ֻ�Ծ,��RTSP request stream��д����θ�����(û�и�ʽ����)error text Message���ͻ���,ͬʱ����QTSS_RTSPRequestAttributes�е��������
   RTSP Status Code,ResponseKeepAlive,qtssRTSPReqRespMsg. �ú����ܷ���QTSS_RequestFailed  */
QTSS_Error	QTSSModuleUtils::SendErrorResponseWithMessage( QTSS_RTSPRequestObject inRequest,
														QTSS_RTSPStatusCode inStatusCode,
														StrPtrLen* inErrorMessagePtr)
{
    static Bool16 sFalse = false;
    
    //set RTSP headers necessary for this error response message
	//set status code for access log,����ָ��RTSP Request��״̬��
    (void)QTSS_SetValue(inRequest, qtssRTSPReqStatusCode, 0, &inStatusCode, sizeof(inStatusCode));
	// tell the server to end the session,���ò����ֻ�Ծ
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespKeepAlive, 0, &sFalse, sizeof(sFalse));
    StrPtrLen theErrorMessage(NULL, 0);
    
    if (sEnableRTSPErrorMsg)
    {   //���ȷ����δ���
		Assert(inErrorMessagePtr != NULL);
		//Assert(inErrorMessagePtr->Ptr != NULL);
		//Assert(inErrorMessagePtr->Len != 0);
		/* �����inErrorMessagePtr��ʼ������������theErrorMessage */
		theErrorMessage.Set(inErrorMessagePtr->Ptr, inErrorMessagePtr->Len);
		
		/* ����εĳ�����ָ����ʽ����buff���� */
        char buff[32];
        qtss_sprintf(buff,"%lu",inErrorMessagePtr->Len);
		/* ��qtssContentLengthHeader��ͷ��Ϣ���ӵ�RTSP��ͷ��,����"Content-length: 1209\r\n" */
        (void)QTSS_AppendRTSPHeader(inRequest, qtssContentLengthHeader, buff, ::strlen(buff));
    }
    
    //send the response header. In all situations where errors could happen, we
    //don't really care, cause there's nothing we can do anyway!
	//�������������theErrorMessage�е�ָ��text Message����RTSPResponseStream��
    (void)QTSS_SendRTSPHeaders(inRequest);

    //
    // Now that we've formatted the message into the temporary buffer,
    // write it out to the request stream and the Client Session object
    (void)QTSS_Write(inRequest, theErrorMessage.Ptr, theErrorMessage.Len, NULL, 0);
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespMsg, 0, theErrorMessage.Ptr, theErrorMessage.Len);
    
    return QTSS_RequestFailed;
}


/* ��RTSP request stream��д��ָ����ʽ��HTTP Error Message��Ϣ,ͬʱ����QTSS_RTSPRequestAttributes�е��������
   RTSP Status Code,ResponseKeepAlive,qtssRTSPReqRespMsg. �ú����ܷ���QTSS_RequestFailed */
QTSS_Error	QTSSModuleUtils::SendHTTPErrorResponse( QTSS_RTSPRequestObject inRequest,
													QTSS_SessionStatusCode inStatusCode,
                                                    Bool16 inKillSession,/* �ر�Session��? */
                                                    char *errorMessage)/* �ṩ��error Message body */
{
    static Bool16 sFalse = false;
    
    //set status code for access log
    (void)QTSS_SetValue(inRequest, qtssRTSPReqStatusCode, 0, &inStatusCode, sizeof(inStatusCode));

    if (inKillSession) // tell the server to end the session
        (void)QTSS_SetValue(inRequest, qtssRTSPReqRespKeepAlive, 0, &sFalse, sizeof(sFalse));

    /********** ע������������buffer�ǳ���Ҫ!! *******************/

    ResizeableStringFormatter theErrorMessage(NULL, 0); //allocates and deletes memory
	/* ���ڵ��ĸ�����error Message Body,�������� */
    ResizeableStringFormatter bodyMessage(NULL,0); //allocates and deletes memory

	/**************************************************************/

	/* ����ÿ��64�ַ�,���һ���ַ�Ϊ0 */
    char messageLineBuffer[64]; // used for each line
    static const int maxMessageBufferChars = sizeof(messageLineBuffer) -1;//63
    messageLineBuffer[maxMessageBufferChars] = 0; // guarantee NULL termination

    // ToDo: put in a more meaningful http error message for each error. Not required by spec.
    // ToDo: maybe use the HTTP protcol class static error strings.
	/* ���ø��������http error message for each "error" */
    char* errorMsg = "error"; 

	/************** ���λ�ȡʱ��,RTSP real Status Code,RTSP Server Header׼������theErrorMessage *************/

	/* ���ڶ������inDate������ʱ,��local��ǰʱ�����ΪGMT.�ٽ�GMTʱ����ָ����ʽ�������theDate */
	/* ��ȡʱ����ϢtheDate */
    DateBuffer theDate;
    DateTranslator::UpdateDateBuffer(&theDate, 0); // get the current GMT date and time

	/* ��ȡRTSP real Status Code */
    UInt32 realCode = 0;
    UInt32 len = sizeof(realCode);
    (void) QTSS_GetValue(inRequest, qtssRTSPReqRealStatusCode, 0,  (void*)&realCode,&len);

	/* ��ȡRTSP Server Header */
    char serverHeaderBuffer[64]; // the qtss Server: header field
    len = sizeof(serverHeaderBuffer) -1; // leave room for terminator
	//Server: header that the server uses to respond to RTSP clients
    (void) QTSS_GetValue(sServer, qtssSvrRTSPServerHeader, 0,  (void*)serverHeaderBuffer,&len);
    serverHeaderBuffer[len] = 0; // terminate.
 
	/**************************** ���������Ϣ��ָ����ʽ����theErrorMessage **************************/

	/* ������"HTTP/1.1 ** error"����һ��buffer(63���ַ���) */
    qtss_snprintf(messageLineBuffer,maxMessageBufferChars, "HTTP/1.1 %lu %s",realCode, errorMsg);
	/* ��ָ������(63)�ĸ�ʽ�ַ���"HTTP/1.1 ** error"���뻺����,ĩβ����"\0" */
    theErrorMessage.Put(messageLineBuffer,::strlen(messageLineBuffer));
    theErrorMessage.PutEOL();


	/* ����ȡ��server Header��Ϣ���뻺����,ĩβ����"\r\n" */
    theErrorMessage.Put(serverHeaderBuffer,::strlen(serverHeaderBuffer));
    theErrorMessage.PutEOL();
 
	/* �����ڰ�ָ����ʽ����messageLineBuffer,��һ������buffer,������"\r\n" */
    qtss_snprintf(messageLineBuffer,maxMessageBufferChars, "Date: %s",theDate.GetDateBuffer());
    theErrorMessage.Put(messageLineBuffer,::strlen(messageLineBuffer));
    theErrorMessage.PutEOL();
 
	/* �������errorMessage�ǿ� */
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

		/* ע������ת������һ���ڴ�theErrorMessage */
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

	/* ���ڽ�bodyMessage�е���ϢҲ�Ž�theErrorMessage */
    if (addBody) // add html body
    {
        theErrorMessage.Put(bodyMessage.GetBufPtr(),bodyMessage.GetBytesWritten());
    }

    //
    // Now that we've formatted the message into the temporary buffer,
    // write it out to the request stream and the Client Session object
	/* ���ڴ��е�formatted HTTP Error Messageд��request RTSP stream */
	/* ���Ǳ���������Ҫ��һ��!! */
    (void)QTSS_Write(inRequest, theErrorMessage.GetBufPtr(), theErrorMessage.GetBytesWritten(), NULL, 0);
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespMsg, 0, theErrorMessage.GetBufPtr(), theErrorMessage.GetBytesWritten());
    
    return QTSS_RequestFailed;
}

/* ��qtssContentLengthHeader��ͷ��Ϣ���ӵ�RTSP��ͷ�в�����������ͷ,�ٽ�ָ����describeData��iovec�ṹд�뵽���� */
void    QTSSModuleUtils::SendDescribeResponse(QTSS_RTSPRequestObject inRequest,
                                                    QTSS_ClientSessionObject inSession,
                                                    iovec* describeData,
                                                    UInt32 inNumVectors,
                                                    UInt32 inTotalLength)
{
    //write content size header
	/* �������һ�����inTotalLength */
    char buf[32];
    qtss_sprintf(buf, "%ld", inTotalLength);
    (void)QTSS_AppendRTSPHeader(inRequest, qtssContentLengthHeader, &buf[0], ::strlen(&buf[0]));

	/* ��һ����׼����Ӧд�뵽��inRequest����ָ����RTSP�� */
    (void)QTSS_SendStandardRTSPResponse(inRequest, inSession, 0);

        // On solaris, the maximum # of vectors is very low (= 16) so to ensure that we are still able to
        // send the SDP if we have a number greater than the maximum allowed, we coalesce(ƴ��) the vectors into
        // a single big buffer

	/* ͨ��iovec�ṹ������д�뵽���У��䷽ʽ������POSIX��writev���á� */
    (void)QTSS_WriteV(inRequest, describeData, inNumVectors, inTotalLength, NULL);
}

/* used in QTSSModuleUtils::SendDescribeResponse() */
/* ��ָ��������(�ṹ��)�����Ԫ����βƴ���������´�����½��ڴ���,�����ڴ���ʼ��ָ�� */
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

/*************** ע���������������ǹ���Module Object������Ե� ****************************/

/* ��ָ����Module object��ȡModulePrefsObject */
QTSS_ModulePrefsObject QTSSModuleUtils::GetModulePrefsObject(QTSS_ModuleObject inModObject)
{
    QTSS_ModulePrefsObject thePrefsObject = NULL;
    UInt32 theLen = sizeof(thePrefsObject);
    QTSS_Error theErr = QTSS_GetValue(inModObject, qtssModPrefs, 0, &thePrefsObject, &theLen);
    Assert(theErr == QTSS_NoErr);
    
    return thePrefsObject;
}

/* ��ָ����Module object��ȡModuleAttributesObject */
QTSS_Object QTSSModuleUtils::GetModuleAttributesObject(QTSS_ModuleObject inModObject)
{
    QTSS_Object theAttributesObject = NULL;
    UInt32 theLen = sizeof(theAttributesObject);
    QTSS_Error theErr = QTSS_GetValue(inModObject, qtssModAttributes, 0, &theAttributesObject, &theLen);
    Assert(theErr == QTSS_NoErr);
    
    return theAttributesObject;
}

/* ��QTSS_ServerAttributes�е�qtssSvrModuleObjects�б������ҷ������ָ��Module Name��������ModuleObject,������ */
QTSS_ModulePrefsObject QTSSModuleUtils::GetModuleObjectByName(const StrPtrLen& inModuleName)
{
    QTSS_ModuleObject theModule = NULL;
    UInt32 theLen = sizeof(theModule);
    
	/* ��QTSS_ServerAttributes�е�qtssSvrModuleObjects�б������ҷ���������ModuleObject */
    for (int x = 0; QTSS_GetValue(sServer, qtssSvrModuleObjects, x, &theModule, &theLen) == QTSS_NoErr; x++)
    {
        Assert(theModule != NULL);
        Assert(theLen == sizeof(theModule));
        
		/* ��ȡ��Module��qtssModName����ֵtheName */
        StrPtrLen theName;
        QTSS_Error theErr = QTSS_GetValuePtr(theModule, qtssModName, 0, (void**)&theName.Ptr, &theName.Len);
        Assert(theErr == QTSS_NoErr);
        
		/* ���ҵ����ָ��Module Name��Module���������� */
        if (inModuleName.Equal(theName))
            return theModule;
            
#if DEBUG
        theModule = NULL;
        theLen = sizeof(theModule);
#endif
    }
    return NULL;
}


/***************************** ע������ĺ����ǹ���Attribute **********************************************************/

/* ���ȼ��ָ�����Ե�DataType,��������ȷ������ID,�ٽ�����ֵ������һ����������.�����ȡ����ֵ����,�Ͱ�default����ֵ(����еĻ�)
   ���뻺����,��¼error log,������һ���µ������ҷ�����������ID */
void    QTSSModuleUtils::GetAttribute(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType, 
                                                void* ioBuffer, void* inDefaultValue, UInt32 inBufferLen)
{
    //
    // Check to make sure this attribute is the right type. If it's not, this will coerce(ǿ��,ǿ��)
    // it to be the right type. This also returns the id of the attribute **����д�ú�!***
	/* ���ָ�����Ե�DataType,��������ȷ������ID(DataType���Ե�Ҫ���´����µ���ȷ������) */
    QTSS_AttributeID theID = QTSSModuleUtils::CheckAttributeDataType(inObject, inAttributeName, inType, inDefaultValue, inBufferLen);

    //
    // Get the attribute value.
	/* ������ֵ������һ����������,ע����Ϊ����������,inBufferLen��ֵ�ѱ�����! */
    QTSS_Error theErr = QTSS_GetValue(inObject, theID, 0, ioBuffer, &inBufferLen);
    
    //
    // Caller should KNOW how big this attribute is
	/* ���ȷ���û������㹻��,����ioBuffer���ǿյ� */
    Assert(theErr != QTSS_NotEnoughSpace);
    
	/* �Ի�ȡ����ֵ������д��� */
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
			/* ��¼��Ԥ��ֵqtssServerPrefMissing��error Log,��·����QTSSModuleUtils::CheckAttributeDataType() */
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

/* ���ȼ��ָ�����Ե�DataType,��������ȷ������ID,�ٻ�ȡָ�����Ե�ֵ����ת��ΪC�ַ�����ʽ������;����ȡ����ֵ����,�ʹ���һ���µ�����,
   ��¼error log,���������inDefaultValue */
char*   QTSSModuleUtils::GetStringAttribute(QTSS_Object inObject, char* inAttributeName, char* inDefaultValue)
{
	/* ��ȡ���inDefaultValue���� */
    UInt32 theDefaultValLen = 0;
    if (inDefaultValue != NULL)
        theDefaultValLen = ::strlen(inDefaultValue);
    
    //
    // Check to make sure this attribute is the right type. If it's not, this will coerce(ǿ��,ǿ��)
    // it to be the right type
	/* ���ָ�����Ե�DataType,��������ȷ������ID(DataType���Ե�Ҫ���´����µ���ȷ������) */
    QTSS_AttributeID theID = QTSSModuleUtils::CheckAttributeDataType(inObject, inAttributeName, qtssAttrDataTypeCharArray, inDefaultValue, theDefaultValLen);

    char* theString = NULL;
	/* ��ȡָ�����Ե�ֵ����ת��ΪC�ַ�����ʽ���洢����&theString����ָ�����ڴ�λ���� */
    (void)QTSS_GetValueAsString(inObject, theID, 0, &theString);
    if (theString != NULL)
        return theString;
    
    //
    // If we get here, the attribute must be missing, so create it and log
    // an error.
    
	/* �����ȡ����ֵ����,�ʹ���һ���µ����� */
    QTSSModuleUtils::CreateAttribute(inObject, inAttributeName, qtssAttrDataTypeCharArray, inDefaultValue, theDefaultValLen);
    
    //
    // Return the default if it was provided. Only log an error if the default value was provided
	/* �����ṩ��inDefaultValue���ȷ���,�ͼ�¼error log */
    if (theDefaultValLen > 0)
    {
        QTSSModuleUtils::LogError(  sMissingPrefVerbosity,
                                    qtssServerPrefMissing,
                                    0,
                                    inAttributeName,
                                    inDefaultValue);
    }
    
    /* �����ṩ�����inDefaultValue�ǿ�,�ͷ��仺�沢������ */
    if (inDefaultValue != NULL)
    {
        //
        // Whether to return the default value or not from this function is dependent
        // solely on whether the caller passed in a non-NULL pointer or not.
        // This ensures that if the caller wants an empty-string returned as a default
        // value, it can do that.
		/* �Ƿ񷵻����inDefaultValueֻ�ɸ�����Ƿ�ǿվ��� */
        theString = NEW char[theDefaultValLen + 1];
        ::strcpy(theString, inDefaultValue);
        return theString;
    }
    return NULL;
}

/* ��QTSSModuleUtils::GetAttribute()��������ͬ,����һ������ */
void    QTSSModuleUtils::GetIOAttribute(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType,
                            void* ioDefaultResultBuffer, UInt32 inBufferLen)
{
	/* ��������洢���ioDefaultResultBuffer */
    char *defaultBuffPtr = NEW char[inBufferLen];
    ::memcpy(defaultBuffPtr,ioDefaultResultBuffer,inBufferLen);
	/* �������,���������ֵ����ͬ�� */
    QTSSModuleUtils::GetAttribute(inObject, inAttributeName, inType, ioDefaultResultBuffer, defaultBuffPtr, inBufferLen);
    delete [] defaultBuffPtr;

}
                            

/* �ȴ����inAttributeName�������ƻ�ȡQTSS_AttrInfoObject�����ٴ���QTSS_AttrInfoObjectAttributes�л������ID.
   ע��inAttributeName�������ƺ�����ID����QTSS_AttrInfoObjectAttributes�� */
QTSS_AttributeID QTSSModuleUtils::GetAttrID(QTSS_Object inObject, char* inAttributeName)
{
    //
    // Get the attribute ID of this attribute.
	/* ͨ���������Ƶõ�һ��QTSS_AttrInfoObject���󣬿���ͨ���ö���ȡ�����Ե����ƺ�ID�����Ե��������ͣ���д����ֵ��Ȩ�ޣ��Լ���ȡ����ֵ�Ĳ����Ƿ���ռ���ʰ�ȫ */
    QTSS_Object theAttrInfo = NULL;
    QTSS_Error theErr = QTSS_GetAttrInfoByName(inObject, inAttributeName, &theAttrInfo);
    if (theErr != QTSS_NoErr)
        return qtssIllegalAttrID;

	/* ��QTSS_AttrInfoObject����������ID */
    QTSS_AttributeID theID = qtssIllegalAttrID; 
    UInt32 theLen = sizeof(theID);
    theErr = QTSS_GetValue(theAttrInfo, qtssAttrID, 0, &theID, &theLen);
    Assert(theErr == QTSS_NoErr);

    return theID;
}

/* ͨ���������Ƶõ�һ��QTSS_AttrInfoObject����,�ٴ���QTSS_AttrInfoObjectAttributes�л�ȡ����DataType,�������inType��ͬ,�����error Log,ɾ�������½���ʵ������,����(�����½���)����ID */
QTSS_AttributeID QTSSModuleUtils::CheckAttributeDataType(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType, void* inDefaultValue, UInt32 inBufferLen)
{
    //
    // Get the attribute type of this attribute.

	/* ͨ���������Ƶõ�һ��QTSS_AttrInfoObject���� */
    QTSS_Object theAttrInfo = NULL;
    QTSS_Error theErr = QTSS_GetAttrInfoByName(inObject, inAttributeName, &theAttrInfo);
    if (theErr != QTSS_NoErr)
        return qtssIllegalAttrID;

	/* ��QTSS_AttrInfoObject����������DataType */
    QTSS_AttrDataType theAttributeType = qtssAttrDataTypeUnknown;
    UInt32 theLen = sizeof(theAttributeType);
    theErr = QTSS_GetValue(theAttrInfo, qtssAttrDataType, 0, &theAttributeType, &theLen);
    Assert(theErr == QTSS_NoErr);
    
    /* ��QTSS_AttrInfoObject����������ID */
    QTSS_AttributeID theID = qtssIllegalAttrID; 
    theLen = sizeof(theID);
    theErr = QTSS_GetValue(theAttrInfo, qtssAttrID, 0, &theID, &theLen);
    Assert(theErr == QTSS_NoErr);

	/* ���ڻ�õ�����DataType�������inType��ͬ */
    if (theAttributeType != inType)
    {
		/* ��һ����ʽΪQTSS_AttrDataType��������������ת��ΪC�ַ�����ʽ��ֵ */
        char* theValueAsString = NULL;
        theErr = QTSS_ValueToString(inDefaultValue, inBufferLen, inType, &theValueAsString);
        Assert(theErr == QTSS_NoErr);

		/* �ַ�����ɾ������� */
        OSCharArrayDeleter theValueStr(theValueAsString);
		/* ��ȡָ��QTSS_AttributeID��������Ϣ(�ַ�����ʽ��Ϣ),������������������ָ����ʽ�����½��Ļ���,����ָ��д���־д��Error Log�� */
        QTSSModuleUtils::LogError(  qtssWarningVerbosity,/* д���ʶ */
                                    qtssServerPrefWrongType,/* �ַ�����ʽ��Ϣ */
                                    0,
                                    inAttributeName,
                                    theValueStr.GetObject());
        /* ��inObject����ָ���Ķ�����ɾ����theID����ָ����ʵ������ */                            
        theErr = QTSS_RemoveInstanceAttribute( inObject, theID );
        Assert(theErr == QTSS_NoErr);
        return  QTSSModuleUtils::CreateAttribute(inObject, inAttributeName, inType, inDefaultValue, inBufferLen);
    }
    return theID;
}

/* ����ָ���Ķ���ʵ�����һ��ʵ������,�ٻ�ȡ����ID,����ø��������inDefaultValue��ʽ����ָ�����Ե�ֵ,��������ID */
QTSS_AttributeID QTSSModuleUtils::CreateAttribute(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType, void* inDefaultValue, UInt32 inBufferLen)
{
	/* ��inObject����ָ���Ķ���ʵ�����һ��ʵ�����ԡ�����ص�����ֻ����Register��ɫ�е��� */
    QTSS_Error theErr = QTSS_AddInstanceAttribute(inObject, inAttributeName, NULL, inType);
	/* ���ָ�����Ƶ������Ѿ�����,�򷵻�QTSS_AttrNameExists */
    Assert((theErr == QTSS_NoErr) || (theErr == QTSS_AttrNameExists));
    
	/* �ȴ����inAttributeName�������ƻ�ȡQTSS_AttrInfoObject�����ٴ���QTSS_AttrInfoObjectAttributes�л������ID.*/
    QTSS_AttributeID theID = QTSSModuleUtils::GetAttrID(inObject, inAttributeName);
    Assert(theID != qtssIllegalAttrID);
        
    //
    // Caller can pass in NULL for inDefaultValue, in which case we don't add the default
	/* ������inDefaultValue�ǿ�,���Ǿ�Ҫ������;����Ͳ��������� */
    if (inDefaultValue != NULL)
    {   
		/* �ø��������inDefaultValue��ʽ����ָ�����Ե�ֵ */
        theErr = QTSS_SetValue(inObject, theID, 0, inDefaultValue, inBufferLen);
        Assert(theErr == QTSS_NoErr);
    }
    return theID;
}

/***�����ĸ������ǻ�ȡQTSS_RTSPRequestAttributes��������Ե�***/

/* ���û�ȡ���Եķ���,�õ�RTSP�����Request Actions Flags */
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

/* ���û�ȡ���Եķ���,�õ�RTSP�����ý���ļ���Local path */
//The full local path to the file. This Attribute is first set after the Routing Role has run and before any other role is called. 
char* QTSSModuleUtils::GetLocalPath_Copy(QTSS_RTSPRequestObject theRTSPRequest)
{   char*   pathBuffStr = NULL;
    QTSS_Error theErr = QTSS_GetValueAsString(theRTSPRequest, qtssRTSPReqLocalPath, 0, &pathBuffStr);
    Assert(theErr == QTSS_NoErr);
    return pathBuffStr;
}

/* ���û�ȡ���Եķ���,�õ�RTSP�����ý���ļ���Root Directory */
//Root directory to use for this request. The default value for this parameter is the server's media folder path. Modules may set this attribute from the QTSS_RTSPRoute_Role.
char* QTSSModuleUtils::GetMoviesRootDir_Copy(QTSS_RTSPRequestObject theRTSPRequest)
{   char*   movieRootDirStr = NULL;
    QTSS_Error theErr = QTSS_GetValueAsString(theRTSPRequest,qtssRTSPReqRootDir, 0, &movieRootDirStr);
    Assert(theErr == QTSS_NoErr);
    return movieRootDirStr;
}

/* ���û�ȡ���Եķ���,�õ�RTSP�����ý���ļ���User Profile */
//Object's username is filled in by the server and its password and group memberships filled in by the authentication module.
QTSS_UserProfileObject QTSSModuleUtils::GetUserProfileObject(QTSS_RTSPRequestObject theRTSPRequest)
{   QTSS_UserProfileObject theUserProfile = NULL;
    UInt32 len = sizeof(QTSS_UserProfileObject);
    QTSS_Error theErr = QTSS_GetValue(theRTSPRequest, qtssRTSPReqUserProfile, 0, (void*)&theUserProfile, &len);
    Assert(theErr == QTSS_NoErr);
    return theUserProfile;
}


/******* ����3�������ǻ�ȡQTSS_UserProfileObjectAttributes��������Ե� *********/

/* ���û�ȡ���Եķ���,�õ�User Name�������� */
char *QTSSModuleUtils::GetUserName_Copy(QTSS_UserProfileObject inUserProfile)
{
    char*   username = NULL;    
    (void) QTSS_GetValueAsString(inUserProfile, qtssUserName, 0, &username);
    return username;
}

/* ���û�ȡ���Եķ���,�õ�User Group���������,����User Group���� */
char**  QTSSModuleUtils::GetGroupsArray_Copy(QTSS_UserProfileObject inUserProfile, UInt32 *outNumGroupsPtr)
{
    Assert(NULL != outNumGroupsPtr);

    char** outGroupCharPtrArray = NULL;
    *outNumGroupsPtr = 0;
    
	/* ȷ����һ�������ǿ� */
    if (NULL == inUserProfile)
        return NULL;
    
	/* ��ȡNumber of User Group */
    QTSS_Error theErr = QTSS_GetNumValues (inUserProfile,qtssUserGroups, outNumGroupsPtr);
    if (theErr != QTSS_NoErr || *outNumGroupsPtr == 0)
        return NULL;
    
	
	/* �½�ָ����С��User Group */
    outGroupCharPtrArray = NEW char*[*outNumGroupsPtr]; // array of char *
    UInt32 len = 0;
    for (UInt32 index = 0; index < *outNumGroupsPtr; index++)
    {   outGroupCharPtrArray[index] = NULL;
	/* ��ȡGroup������ÿ������(char����)������ */
        QTSS_GetValuePtr(inUserProfile, qtssUserGroups, index,(void **) &outGroupCharPtrArray[index], &len);
    }   

    return outGroupCharPtrArray;
}

/* �����ȡuser group�ַ���,�������Ƿ��������inGroup��ȵ�?�ҵ����ж��˳�������true,���򷵻�false.ע�����������û�õ� */
Bool16 QTSSModuleUtils::UserInGroup(QTSS_UserProfileObject inUserProfile, char* inGroup, UInt32 inGroupLen)
{
	/* ȷ����ζ��ǿ� */
	if (NULL == inUserProfile || NULL == inGroup  ||  inGroupLen == 0) 
		return false;
	
	/* ��ȡuser name���� */
	char *userName = NULL;
	UInt32 len = 0;
	QTSS_GetValuePtr(inUserProfile, qtssUserName, 0, (void **)&userName, &len);
	if (len == 0 || userName == NULL || userName[0] == 0) // no user to check
		return false;

	/* ��ȡuser group �������� */
	UInt32 numGroups = 0;
	QTSS_GetNumValues (inUserProfile,qtssUserGroups, &numGroups);
	if (numGroups == 0) // no groups to check
		return false;

	Bool16 result = false;
	char* userGroup = NULL;
	StrPtrLenDel userGroupStr; //deletes pointer in destructor
	
	/* �����ȡuser group�ַ���,�������Ƿ��������inGroup��ȵ�?�ҵ����ж��˳�������true */
	for (UInt32 index = 0; index < numGroups; index++)
	{  
		userGroup = NULL;
		/* ��ȡuser group�ַ��� */
		QTSS_GetValueAsString(inUserProfile, qtssUserGroups, index, &userGroup); //allocates string
		/* ����Ա��´�ʹ�� */
		userGroupStr.Delete();
		/* ����Ϊ��ȡ��user group�ַ��� */
		userGroupStr.Set(userGroup);
		/* ���������inGroup��� */
		if(userGroupStr.Equal(inGroup))
		{	
			result = true;
			break;
		}
	}   

	return result;
	
}


/* ��һ�Ƚ����AttributeID�õ���IP Address�����*inAddressPtr�Ƿ����,��ȷ���true,���򷵻�false */
Bool16 QTSSModuleUtils::AddressInList(QTSS_Object inObject, QTSS_AttributeID listID, StrPtrLen *inAddressPtr)
{
    StrPtrLenDel strDeleter;
	/* used in QTSS_GetValueAsString() */
    char*   theAttributeString = NULL; 
	/* ����δ��ݽ����Ա�Ƚ�,�õ�IPComponentStr::IPComponentStr()��IPComponentStr::Set() */
    IPComponentStr inAddress(inAddressPtr);
	/* �������AttributeID�õ���IP Address */
    IPComponentStr addressFromList;
    
	/* �Ϸ��Լ�� */
    if (!inAddress.Valid())
        return false;

	/* �õ�ָ��AttributeID��ֵ���� */
    UInt32 numValues = 0;
    (void) QTSS_GetNumValues(inObject, listID, &numValues);
    
    for (UInt32 index = 0; index < numValues; index ++)
    { 
		/* ����Ա��������� */
        strDeleter.Delete();
		/* ��ȡָ��ID��ָ����ŵ����� */
        (void) QTSS_GetValueAsString(inObject, listID, index, &theAttributeString);
		/* ��ʱ�洢ָ��ID��ָ����ŵ����� */
        strDeleter.Set(theAttributeString);
 
		/* ����ΪIPComponentStr����,�Ա��Ƚ� */
        addressFromList.Set(&strDeleter);
		/* �Ƚ�����IP��ַ�Ƿ���ͬ? */
        if (addressFromList.Equal(&inAddress))
            return true;
    }

    return false;
}

/* used in QTSSFileModule::DoDescribe() */
/* ��һ�Ƚ����AttributeID�õ�������ֵ�����*inStrPtr�Ƿ����,��ȷ���true,���򷵻�false */
Bool16 QTSSModuleUtils::FindStringInAttributeList(QTSS_Object inObject, QTSS_AttributeID listID, StrPtrLen *inStrPtr)
{
    StrPtrLenDel tempString;
    
	/* ȷ����ζ��ǿ� */
    if (NULL == inStrPtr || NULL == inStrPtr->Ptr || 0 == inStrPtr->Len)
        return false;

	/* ���ҿͻ��Ự�������б����м���ֵ */
    UInt32 numValues = 0;
    (void) QTSS_GetNumValues(inObject, listID, &numValues);
    
    for (UInt32 index = 0; index < numValues; index ++)
    { 
		/* ����������н���Ա��´�ʹ�� */
        tempString.Delete();
		/* ȡ��ÿ�ε�����ֵ */
        (void) QTSS_GetValueAsString(inObject, listID, index, &tempString.Ptr);
		/* ��������Ϊȡ��������ֵ */
        tempString.Set(tempString.Ptr);

		/* ����ܲ��ҵ��ַ��� */
        if (inStrPtr->FindString(tempString.Ptr))
            return true;
            
   }

    return false;
}

/* used in QTSSFileModule::DoDescribe() */
/* ���ȴ�RTP Session�л���ַ���userAgentStr,�ٶ����feature����������,�����userAgentStr�Ƿ���Ԥ��ֵ��������featureָ���������б���,��������,����false */
Bool16 QTSSModuleUtils::HavePlayerProfile(QTSS_PrefsObject inPrefObjectToCheck, QTSS_StandardRTSP_Params* inParams, UInt32 feature)
{
	/* ��ȡRTP Session�е�����qtssCliSesFirstUserAgent */
    StrPtrLenDel userAgentStr;    	
    (void)QTSS_GetValueAsString(inParams->inClientSession, qtssCliSesFirstUserAgent, 0, &userAgentStr.Ptr);
    userAgentStr.Set(userAgentStr.Ptr);//�ҵ�����Զ�ɾ��
    
	/* ����η�����(ֻ����������)����: */
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

/****************** ��������IPComponentStr�ĳ�Ա�������� *************************/

IPComponentStr IPComponentStr::sLocalIPCompStr("127.0.0.*");

IPComponentStr::IPComponentStr(char *theAddressPtr)
{
    StrPtrLen sourceStr(theAddressPtr);
	/* ʵ����������fIsValid */
     (void) this->Set(&sourceStr);    
}

IPComponentStr::IPComponentStr(StrPtrLen *sourceStrPtr)
{
	/* ʵ����������fIsValid */
    (void) this->Set(sourceStrPtr);    
}

/* ����δӵ�һ��������ʼѭ���Ƚ��Ƿ�����fAddressComponent[3]��ͬ��,�о�����fIsValidΪtrue,�˳�,��������ֵΪtrue,���򷵻�false */
Bool16 IPComponentStr::Set(StrPtrLen *theAddressStrPtr)
{
    fIsValid = false;
   
    StringParser IP_Paser(theAddressStrPtr);
	/* �����������ֵ,����Ḳ�Ǹ�ֵ */
    StrPtrLen *piecePtr = &fAddressComponent[0];

    while (IP_Paser.GetDataRemaining() > 0) 
    {
		/* fStartGetָ�������ַ�,ֱ������'.',�������Ĳ��ָ���piecePtr(ԭ����ֵ��ˢ��) */
        IP_Paser.ConsumeUntil(piecePtr,'.');

		/* ���õ������IP component��NULL,���ж�ѭ��,����fIsValid = false */
        if (piecePtr->Len == 0) 
            break;

        /* fStartGetָ����ǰ��һ���ַ�,������'.' */
        IP_Paser.ConsumeLength(NULL, 1);

        /* ���õ��ķ�����fAddressComponent[3]��ͬʱ,����fIsValidΪtrue,�˳�,��������ֵΪtrue */
        if (piecePtr == &fAddressComponent[IPComponentStr::kNumComponents -1])
        {
           fIsValid = true;
           break;
        }
        
		/* �Ƶ�fAddressComponent[]����һ������ */
        piecePtr++;
    };
     
    return fIsValid;
}

/* ��һѭ���Ƚ�������IP address����4�������Ƿ����,������Ⱦͷ���true,���򷵻�false */
Bool16 IPComponentStr::Equal(IPComponentStr *testAddressPtr)
{
	/* ȷ����ηǿ� */
    if (testAddressPtr == NULL) 
        return false;
    
	/* ȷ�������ַ������ǺϷ��� */
    if ( !this->Valid() || !testAddressPtr->Valid() )
        return false;

	/* ѭ������������IP address����4������,��һ�Ƚ� */
    for (UInt16 component= 0 ; component < IPComponentStr::kNumComponents ; component ++)
    {
		/* �ֱ�õ�������IP address�ж�Ӧ�ķ��� */
        StrPtrLen *allowedPtr = this->GetComponent(component);
        StrPtrLen *testPtr = testAddressPtr->GetComponent(component);
        
		/* ����IP����Ϊ*,�ͼ���ǰ�� */
        if ( testPtr->Equal("*") || allowedPtr->Equal("*") )
            continue;
         
		/* �Ƚ϶�Ӧ��IP�����Ƿ����? */
        if (!testPtr->Equal(*allowedPtr) ) 
            return false; 
    };  
    
    return true;
}


