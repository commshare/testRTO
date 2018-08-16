
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

//常用数据类型字符串TypeString,参见QTSS_AttrDataType in QTSS.h
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

//十六进制字符
static const char* kHEXChars={ "0123456789ABCDEF" };

//十六进字符和ansi字符对照表
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

//对入参指定的索引,返回对应的数据类型字符串TypeString
char*   QTSSDataConverter::TypeToTypeString( QTSS_AttrDataType inType)
{
    if (inType < qtssAttrDataTypeNumTypes)
        return kDataTypeStrings[inType];
    return kDataTypeStrings[qtssAttrDataTypeUnknown];
}

/* 对入参指定的数据类型字符串TypeString,将其与数组kDataTypeStrings[]中各分量做比较,若相同,返回
   对应的枚举索引,否则返回0 */
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
/* 分三种情形讨论:(1)假如数据类型是字符数组,将入参字符串复制进指定内存,并设置实际数据的长度; (2)假如数据类型是Bool16,
   判断入参指定的字符串是否是"true",是就在缓存中存入true,否则在缓存中存入false; (3)假如是其他数据类型,利用sscanf()转换
   为指定的数据类型,并存入指定缓存 */
QTSS_Error QTSSDataConverter::StringToValue(    char* inValueAsString,
                                                QTSS_AttrDataType inType,
                                                void* ioBuffer,
                                                UInt32* ioBufSize)
{
    UInt32 theBufSize = 0;
    char* theFormat = NULL; //格式化字符串

    if ( inValueAsString == NULL || ioBufSize == NULL)
        return QTSS_BadArgument;

	// If this data type is a string, copy the string into the destination buffer
	//假如数据类型是字符数组,将入参字符串复制进指定内存,并设置实际数据的长度
    if ( inType == qtssAttrDataTypeCharArray )
    {
        UInt32 theLen = ::strlen(inValueAsString);
        
        //首先检查目标内存是否足够大,若不足够大,就返回QTSS_NotEnoughSpace
        // First check to see if the destination is big enough
        if ((ioBuffer == NULL) || (*ioBufSize < theLen))
        {
            *ioBufSize = theLen;
            return QTSS_NotEnoughSpace;
        }
        
        //将入参字符串复制进指定内存,并设置实际数据的长度,返回 
        // Do the string copy. Use memcpy for speed.
        ::memcpy(ioBuffer, inValueAsString, theLen);
        *ioBufSize = theLen;
        return QTSS_NoErr;
    }
    
	//假如数据类型是Bool16,判断入参指定的字符串是否是"true",是就在缓存中存入true,否则在缓存中存入false
    if (inType == qtssAttrDataTypeBool16)
    {
        // The text "enabled" means true, anything else means false
		//检查内存空间是否足够?不够,返回QTSS_NotEnoughSpace
        if (*ioBufSize < sizeof(Bool16))
        {
            *ioBufSize = sizeof(Bool16);
            return QTSS_NotEnoughSpace;
        }
        
        Bool16* it = (Bool16*)ioBuffer;
        //StrPtrLen theValuePtr(inValueAsString);
		//判断入参指定的字符串是否是"true",是就在缓存中存入true,否则在缓存中存入false
        if (kEnabledStr.EqualIgnoreCase(inValueAsString, ::strlen(inValueAsString)))
            *it = true;
        else
            *it = false;
        //设置缓存中实际数据的长度
        *ioBufSize = sizeof(Bool16);
        return QTSS_NoErr;
    }
    
    // 假如是其他数据类型,利用sscanf()转换为指定的数据类型,并存入指定缓存
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
			//默认情况下,将一个字符串中的每个字符转换为16进制字符,存入指定缓存
            return ConvertCHexStringToBytes(inValueAsString,ioBuffer,ioBufSize);
    }

	//确保入参提供的缓存合法
    if (( ioBuffer == NULL) || (*ioBufSize < theBufSize ))
    {
        *ioBufSize = theBufSize;
        return QTSS_NotEnoughSpace;
    }

    *ioBufSize = theBufSize;
	//入参字符串根据变量格式化字符串来转换数据,将转换后的数据存入指定缓存
    ::sscanf(inValueAsString, theFormat, ioBuffer);
    
    return QTSS_NoErr;
}

/* 注意该算法 */
//将一个字符串中的每个字符转换为16进制字符,相邻的两个16进制字符组成一个字节,存入指定缓存,最后调整缓存的长度为原字符串长度的一半
QTSS_Error QTSSDataConverter::ConvertCHexStringToBytes(  char* inValueAsString,
                                                         void* ioBuffer,
                                                         UInt32* ioBufSize)
{
	//获取入参字符串的长度,并适当调整后得到其长度的一半,作为缓存长度
    UInt32 stringLen = ::strlen(inValueAsString) ;
    UInt32 dataLen = (stringLen + (stringLen & 1 ? 1 : 0)) / 2;

    // First check to see if the destination is big enough
	// 检查入参指定的缓存是否足够大?
    if ((ioBuffer == NULL) || (*ioBufSize < dataLen))
    {
        *ioBufSize = dataLen;
        return QTSS_NotEnoughSpace;
    }
    
	//将入参字符串中的每个字符转换为16进制字符,相邻的两个16进制字符组成一个字节,存入指定缓存
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
    //调整缓存的长度
    *ioBufSize = dataLen;
    return QTSS_NoErr;
}

/* 注意该算法 */
//将指定字符串变为16进制字符串,其长度是指定字符串长度的2倍,并返回该新建缓存(注意没有释放)
char* QTSSDataConverter::ConvertBytesToCHexString( void* inValue, const UInt32 inValueLen)
{
    UInt8* theDataPtr = (UInt8*) inValue;
    UInt32 len = inValueLen *2;
    
	//新建缓存来存放16进字符串
    char *theString = NEW char[len+1];
    char *resultStr = theString;
	/* 将入参字符串中每个字符,转换为两个16进制字符存入新建缓存(最高的4bit先存),最后一个字符为0 */
    if (theString != NULL)
    {
        UInt8 temp;
        UInt32 count = 0;
        for (count = 0; count < inValueLen; count++)
        {
            temp = *theDataPtr++;
            *theString++ = kHEXChars[temp >> 4]; //将最高的4bit转换为16进制字符,存入缓存
            *theString++ = kHEXChars[temp & 0xF];//将最低的4bit转换为16进制字符,存入缓存
        }
        *theString = 0; //最后一个字符为0
    }
    return resultStr;
}
 
/* used in QTSSDictionary::GetValueAsString() */
/* 将指定的数据类型(不一定是NULL结尾的),转换为指定格式的C-string(必定是NULL-terminated),存入新建的缓存并返回,分为三种类型:
 (1)假如指定的数据类型是字符数组,返回相应的C-String; (2)假如指定的数据类型是Bool16,若非空,返回"true",否则返回"false";(3)假
  如是其他数据类型,先分配128个字符的缓存,再转换为指定格式的字符串 */
char* QTSSDataConverter::ValueToString( void* inValue,
                                        const UInt32 inValueLen,
                                        const QTSS_AttrDataType inType)
{
	//假如没有指定入参,返回NULL
    if (inValue == NULL)
        return NULL;

	//假如指定的数据类型是字符数组,返回相应的C-String
    if ( inType == qtssAttrDataTypeCharArray )
    {
        StrPtrLen theStringPtr((char*)inValue, inValueLen);
        return theStringPtr.GetAsCString();
    } 

	//假如指定的数据类型是Bool16,若非空,返回"true",否则返回"false"
    if (inType == qtssAttrDataTypeBool16)
    {
        Bool16* theBoolPtr = (Bool16*)inValue;
        if (*theBoolPtr)
            return kEnabledStr.GetAsCString();
        else
            return kDisabledStr.GetAsCString();
    }
    
    // 假如是其他数据类型，先分配128个字符的缓存,再转换为指定格式的字符串
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

		//默认处理，先删除动态分配的缓存，并将指定字符串变为16进制字符串，存入另外新建的缓存
        default:
            delete theString;
            theString = ConvertBytesToCHexString(inValue, inValueLen);
    }

    return theString;
}
