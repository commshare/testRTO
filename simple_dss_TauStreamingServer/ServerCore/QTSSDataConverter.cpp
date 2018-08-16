
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSDataConverter.cpp
Description: Utility routines for converting to and from QTSS_AttrDataTypes and text
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include "QTSSDataConverter.h"
#include "StrPtrLen.h"
#include "OSMemory.h"
#include <string.h>
#include <stdio.h>


static const StrPtrLen kEnabledStr("true");
static const StrPtrLen kDisabledStr("false");

//�������������ַ���TypeString,�μ�QTSS_AttrDataType in QTSS.h
static char* kDataTypeStrings[] =
{
    "Unknown",
    "CharArray",
    "Bool16",
    "SInt16",
    "UInt16",
    "SInt32",
    "UInt32",
    "SInt64",
    "UInt64",
    "QTSS_Object",
    "QTSS_StreamRef",
    "Float32",
    "Float64",
    "VoidPointer",
    "QTSS_TimeVal"
};

//ʮ�������ַ�
static const char* kHEXChars={ "0123456789ABCDEF" };

//ʮ�����ַ���ansi�ַ����ձ�
static const UInt8 sCharToNums[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //0-9 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //10-19 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //20-29
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //30-39 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, //40-49 --- 48-57 = '0'-'9'
    2, 3, 4, 5, 6, 7, 8, 9, 0, 0, //50-59
    0, 0, 0, 0, 0, 10,11,12,13,14,//60-69 --- 65-69 = A-E
    15,0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79 ----70 = F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
    0, 0, 0, 0, 0, 0, 0,10, 11,12,//90-99 --- 97-102 = a-f
    13,14,15,0, 0, 0, 0, 0, 0, 0, //100-109
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //120-129
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //130-139
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //140-149
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //150-159
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //160-169
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //170-179
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //180-189
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //190-199
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //200-209
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //210-219
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //220-229
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //230-239
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //240-249
    0, 0, 0, 0, 0, 0              //250-255
};

//�����ָ��������,���ض�Ӧ�����������ַ���TypeString
char*   QTSSDataConverter::TypeToTypeString( QTSS_AttrDataType inType)
{
    if (inType < qtssAttrDataTypeNumTypes)
        return kDataTypeStrings[inType];
    return kDataTypeStrings[qtssAttrDataTypeUnknown];
}

/* �����ָ�������������ַ���TypeString,����������kDataTypeStrings[]�и��������Ƚ�,����ͬ,����
   ��Ӧ��ö������,���򷵻�0 */
QTSS_AttrDataType QTSSDataConverter::TypeStringToType( char* inTypeString)
{
    for (UInt32 x = 0; x < qtssAttrDataTypeNumTypes; x++)
    {
        StrPtrLen theTypeStrPtr(inTypeString);
        if (theTypeStrPtr.EqualIgnoreCase(kDataTypeStrings[x], ::strlen(kDataTypeStrings[x])))
            return x;
    }
    return qtssAttrDataTypeUnknown;
}

/* used in QTSSPrefs::SetPrefValue() */
/* ��������������:(1)���������������ַ�����,������ַ������ƽ�ָ���ڴ�,������ʵ�����ݵĳ���; (2)��������������Bool16,
   �ж����ָ�����ַ����Ƿ���"true",�Ǿ��ڻ����д���true,�����ڻ����д���false; (3)������������������,����sscanf()ת��
   Ϊָ������������,������ָ������ */
QTSS_Error QTSSDataConverter::StringToValue(    char* inValueAsString,
                                                QTSS_AttrDataType inType,
                                                void* ioBuffer,
                                                UInt32* ioBufSize)
{
    UInt32 theBufSize = 0;
    char* theFormat = NULL; //��ʽ���ַ���

    if ( inValueAsString == NULL || ioBufSize == NULL)
        return QTSS_BadArgument;

	// If this data type is a string, copy the string into the destination buffer
	//���������������ַ�����,������ַ������ƽ�ָ���ڴ�,������ʵ�����ݵĳ���
    if ( inType == qtssAttrDataTypeCharArray )
    {
        UInt32 theLen = ::strlen(inValueAsString);
        
        //���ȼ��Ŀ���ڴ��Ƿ��㹻��,�����㹻��,�ͷ���QTSS_NotEnoughSpace
        // First check to see if the destination is big enough
        if ((ioBuffer == NULL) || (*ioBufSize < theLen))
        {
            *ioBufSize = theLen;
            return QTSS_NotEnoughSpace;
        }
        
        //������ַ������ƽ�ָ���ڴ�,������ʵ�����ݵĳ���,���� 
        // Do the string copy. Use memcpy for speed.
        ::memcpy(ioBuffer, inValueAsString, theLen);
        *ioBufSize = theLen;
        return QTSS_NoErr;
    }
    
	//��������������Bool16,�ж����ָ�����ַ����Ƿ���"true",�Ǿ��ڻ����д���true,�����ڻ����д���false
    if (inType == qtssAttrDataTypeBool16)
    {
        // The text "enabled" means true, anything else means false
		//����ڴ�ռ��Ƿ��㹻?����,����QTSS_NotEnoughSpace
        if (*ioBufSize < sizeof(Bool16))
        {
            *ioBufSize = sizeof(Bool16);
            return QTSS_NotEnoughSpace;
        }
        
        Bool16* it = (Bool16*)ioBuffer;
        //StrPtrLen theValuePtr(inValueAsString);
		//�ж����ָ�����ַ����Ƿ���"true",�Ǿ��ڻ����д���true,�����ڻ����д���false
        if (kEnabledStr.EqualIgnoreCase(inValueAsString, ::strlen(inValueAsString)))
            *it = true;
        else
            *it = false;
        //���û�����ʵ�����ݵĳ���
        *ioBufSize = sizeof(Bool16);
        return QTSS_NoErr;
    }
    
    // ������������������,����sscanf()ת��Ϊָ������������,������ָ������
    // If this is another type, format the string into that type
    switch ( inType )
    {
        case qtssAttrDataTypeUInt16:
        {
            theBufSize = sizeof(UInt16);
            theFormat = "%hu";
        }
        break;
        
        case qtssAttrDataTypeSInt16:
        {
            theBufSize = sizeof(SInt16);
            theFormat = "%hd";
        }
        break;
        
        case qtssAttrDataTypeSInt32:
        {
            theBufSize = sizeof(SInt32);
            theFormat = "%ld";
        }
        break;
        
        case qtssAttrDataTypeUInt32:
        {
            theBufSize = sizeof(UInt32);
            theFormat = "%lu";
        }
        break;
        
        case qtssAttrDataTypeSInt64:
        {
            theBufSize = sizeof(SInt64);
            theFormat = "%"_64BITARG_"d";
        }
        break;
        
        case qtssAttrDataTypeUInt64:
        {
            theBufSize = sizeof(UInt64);
            theFormat = "%"_64BITARG_"u";
        }
        break;
        
        case qtssAttrDataTypeFloat32:
        {
            theBufSize = sizeof(Float32);
            theFormat = "%f";
        }
        break;
        
        case qtssAttrDataTypeFloat64:
        {
            theBufSize = sizeof(Float64);
            theFormat = "%f";
        }
        break;

        case qtssAttrDataTypeTimeVal:
        {
            theBufSize = sizeof(SInt64);
            theFormat = "%"_64BITARG_"d";
        }
        break;

        default:
			//Ĭ�������,��һ���ַ����е�ÿ���ַ�ת��Ϊ16�����ַ�,����ָ������
            return ConvertCHexStringToBytes(inValueAsString,ioBuffer,ioBufSize);
    }

	//ȷ������ṩ�Ļ���Ϸ�
    if (( ioBuffer == NULL) || (*ioBufSize < theBufSize ))
    {
        *ioBufSize = theBufSize;
        return QTSS_NotEnoughSpace;
    }

    *ioBufSize = theBufSize;
	//����ַ������ݱ�����ʽ���ַ�����ת������,��ת��������ݴ���ָ������
    ::sscanf(inValueAsString, theFormat, ioBuffer);
    
    return QTSS_NoErr;
}

/* ע����㷨 */
//��һ���ַ����е�ÿ���ַ�ת��Ϊ16�����ַ�,���ڵ�����16�����ַ����һ���ֽ�,����ָ������,����������ĳ���Ϊԭ�ַ������ȵ�һ��
QTSS_Error QTSSDataConverter::ConvertCHexStringToBytes(  char* inValueAsString,
                                                         void* ioBuffer,
                                                         UInt32* ioBufSize)
{
	//��ȡ����ַ����ĳ���,���ʵ�������õ��䳤�ȵ�һ��,��Ϊ���泤��
    UInt32 stringLen = ::strlen(inValueAsString) ;
    UInt32 dataLen = (stringLen + (stringLen & 1 ? 1 : 0)) / 2;

    // First check to see if the destination is big enough
	// ������ָ���Ļ����Ƿ��㹻��?
    if ((ioBuffer == NULL) || (*ioBufSize < dataLen))
    {
        *ioBufSize = dataLen;
        return QTSS_NotEnoughSpace;
    }
    
	//������ַ����е�ÿ���ַ�ת��Ϊ16�����ַ�,���ڵ�����16�����ַ����һ���ֽ�,����ָ������
    UInt8* dataPtr = (UInt8*) ioBuffer;
    UInt8 char1, char2;
    while (*inValueAsString)
    {   
        char1 = sCharToNums[*inValueAsString++] * 16;
        if (*inValueAsString != 0)
            char2 = sCharToNums[*inValueAsString++];
        else 
            char2 = 0;
        *dataPtr++ = char1 + char2;
    }
    //��������ĳ���
    *ioBufSize = dataLen;
    return QTSS_NoErr;
}

/* ע����㷨 */
//��ָ���ַ�����Ϊ16�����ַ���,�䳤����ָ���ַ������ȵ�2��,�����ظ��½�����(ע��û���ͷ�)
char* QTSSDataConverter::ConvertBytesToCHexString( void* inValue, const UInt32 inValueLen)
{
    UInt8* theDataPtr = (UInt8*) inValue;
    UInt32 len = inValueLen *2;
    
	//�½����������16���ַ���
    char *theString = NEW char[len+1];
    char *resultStr = theString;
	/* ������ַ�����ÿ���ַ�,ת��Ϊ����16�����ַ������½�����(��ߵ�4bit�ȴ�),���һ���ַ�Ϊ0 */
    if (theString != NULL)
    {
        UInt8 temp;
        UInt32 count = 0;
        for (count = 0; count < inValueLen; count++)
        {
            temp = *theDataPtr++;
            *theString++ = kHEXChars[temp >> 4]; //����ߵ�4bitת��Ϊ16�����ַ�,���뻺��
            *theString++ = kHEXChars[temp & 0xF];//����͵�4bitת��Ϊ16�����ַ�,���뻺��
        }
        *theString = 0; //���һ���ַ�Ϊ0
    }
    return resultStr;
}
 
/* used in QTSSDictionary::GetValueAsString() */
/* ��ָ������������(��һ����NULL��β��),ת��Ϊָ����ʽ��C-string(�ض���NULL-terminated),�����½��Ļ��沢����,��Ϊ��������:
 (1)����ָ���������������ַ�����,������Ӧ��C-String; (2)����ָ��������������Bool16,���ǿ�,����"true",���򷵻�"false";(3)��
  ����������������,�ȷ���128���ַ��Ļ���,��ת��Ϊָ����ʽ���ַ��� */
char* QTSSDataConverter::ValueToString( void* inValue,
                                        const UInt32 inValueLen,
                                        const QTSS_AttrDataType inType)
{
	//����û��ָ�����,����NULL
    if (inValue == NULL)
        return NULL;

	//����ָ���������������ַ�����,������Ӧ��C-String
    if ( inType == qtssAttrDataTypeCharArray )
    {
        StrPtrLen theStringPtr((char*)inValue, inValueLen);
        return theStringPtr.GetAsCString();
    } 

	//����ָ��������������Bool16,���ǿ�,����"true",���򷵻�"false"
    if (inType == qtssAttrDataTypeBool16)
    {
        Bool16* theBoolPtr = (Bool16*)inValue;
        if (*theBoolPtr)
            return kEnabledStr.GetAsCString();
        else
            return kDisabledStr.GetAsCString();
    }
    
    // �����������������ͣ��ȷ���128���ַ��Ļ���,��ת��Ϊָ����ʽ���ַ���
    // With these other types, its impossible to tell how big they'll
    // be, so just allocate some buffer and hope we fit.
    char* theString = NEW char[128];
    
    // If this is another type, format the string into that type
    switch ( inType )
    {
        case qtssAttrDataTypeUInt16:
            qtss_sprintf(theString, "%hu", *( UInt16*)inValue);
            break;

        case qtssAttrDataTypeSInt16:
            qtss_sprintf(theString, "%hd", *( SInt16*)inValue);
            break;
        
        case qtssAttrDataTypeSInt32:
            qtss_sprintf(theString, "%ld", *( SInt32*)inValue);
            break;
        
        case qtssAttrDataTypeUInt32:
            qtss_sprintf(theString, "%lu", *( UInt32*)inValue);
            break;
        
        case qtssAttrDataTypeSInt64:
            qtss_sprintf(theString, "%"_64BITARG_"d", *( SInt64*)inValue);
            break;
        
        case qtssAttrDataTypeUInt64:
            qtss_sprintf(theString, "%"_64BITARG_"u", *( UInt64*)inValue);
            break;
        
        case qtssAttrDataTypeFloat32:
            qtss_sprintf(theString, "%f", *( Float32*)inValue);
            break;
        
        case qtssAttrDataTypeFloat64:
            qtss_sprintf(theString, "%f", *( Float64*)inValue);
            break;

        case qtssAttrDataTypeTimeVal:
            qtss_sprintf(theString, "%"_64BITARG_"d", *( SInt64*)inValue);
            break;

		//Ĭ�ϴ�����ɾ����̬����Ļ��棬����ָ���ַ�����Ϊ16�����ַ��������������½��Ļ���
        default:
            delete theString;
            theString = ConvertBytesToCHexString(inValue, inValueLen);
    }

    return theString;
}
