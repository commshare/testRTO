/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPMetaInfoPacket.cpp
Description: Some defs for RTP-Meta-Info payloads. This class also parses RTP meta info packets.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 



#include "RTPMetaInfoPacket.h"
#include "MyAssert.h"
#include "StringParser.h"
#include "OS.h"
#include <string.h>
#include <netinet/in.h>


/* 两个重要的数据成员定义 */
const RTPMetaInfoPacket::FieldName RTPMetaInfoPacket::kFieldNameMap[] =
{
    TW0_CHARS_TO_INT('p', 'p'),
    TW0_CHARS_TO_INT('t', 't'),
    TW0_CHARS_TO_INT('f', 't'),
    TW0_CHARS_TO_INT('p', 'n'),
    TW0_CHARS_TO_INT('s', 'q'),
    TW0_CHARS_TO_INT('m', 'd')
};

/* used in RTPMetaInfoPacket::ParsePacket() */
const UInt32 RTPMetaInfoPacket::kFieldLengthValidator[] =
{
    8,  //pp
    8,  //tt
    2,  //ft
    8,  //pn
    2,  //sq
    0,  //md
    0   //illegal / unknown
};


/* 从FieldName获得对应的FieldIndex,用到RTPMetaInfoPacket::kFieldNameMap[]定义 */
RTPMetaInfoPacket::FieldIndex RTPMetaInfoPacket::GetFieldIndexForName(FieldName inName)
{
    for (int x = 0; x < kNumFields; x++)
    {
        if (inName == kFieldNameMap[x])
            return x;
    }
    return kIllegalField;
}


/* 从入参RTP净负荷元信息的报头中提取相关数据创建FieldIDArray */
void RTPMetaInfoPacket::ConstructFieldIDArrayFromHeader(StrPtrLen* inHeader, FieldID* ioFieldIDArray)
{
	/* 先初始化数组ioFieldIDArray[]的各个分量SInt32,共6个分量 */
    for (UInt32 x = 0; x < kNumFields; x++)
        ioFieldIDArray[x] = kFieldNotUsed;
    
    //
    // Walk through the fields in this header
	/* 传递进入参的值，用于提取相关数据创建FieldIDArray */
    StringParser theParser(inHeader);
    
    UInt16 fieldNameValue = 0;

    while (theParser.GetDataRemaining() > 0)
    {
        StrPtrLen theFieldP;
		/* 指针fStartGet经过';'时将经过的字符串给theFieldP,再越过 ';' */
        (void)theParser.GetThru(&theFieldP, ';');
        
        //
        // Corrupt or something... just bail
		/* 每个Field长度至少为3(2个字节的fieldNameValue(UInt16)+"="+FieldID(SInt32)+":"),参见QTSSModuleUtils::AppendRTPMetaInfoHeader()得到 */
        if (theFieldP.Len < 2)
            break;
        
        //
        // Extract the Field Name and convert it to a Field Index
        ::memcpy (&fieldNameValue, theFieldP.Ptr, sizeof(UInt16));
        FieldIndex theIndex = RTPMetaInfoPacket::GetFieldIndexForName(ntohs(fieldNameValue));
//      FieldIndex theIndex = RTPMetaInfoPacket::GetFieldIndexForName(ntohs(*(UInt16*)theFieldP.Ptr));

        //
        // Get the Field ID if there is one.

		/* temp field ID with initial value of each Field */
        FieldID theID = kUncompressed;
		/* 假如取得的field长度>3,就进一步分解以提取Field ID */
        if (theFieldP.Len > 3)
        {
            StringParser theIDExtractor(&theFieldP);
			/* 指针fStartGet先经过3个字节 */
            theIDExtractor.ConsumeLength(NULL, 3);
			/* 提取后面的数字 */
            theID = theIDExtractor.ConsumeInteger(NULL);
        }
        
		/* 仅当合法Field才赋予Field ID */
        if (theIndex != kIllegalField)
            ioFieldIDArray[theIndex] = theID;
    }
}

/* 分析入参inPacketBuffer中的数据,解析各Field的值,针对不同的域配置相应数据成员的值(共6项) */
Bool16 RTPMetaInfoPacket::ParsePacket(UInt8* inPacketBuffer, UInt32 inPacketLen, FieldID* inFieldIDArray)
{
	/* 标记处RTPMetaInfoPacket在缓存中的起点和终点指针 */
    UInt8* theFieldP = inPacketBuffer + 12; // skip RTP header
    UInt8* theEndP = inPacketBuffer + inPacketLen;
    
	/* temp variables */
    SInt64 sInt64Val = 0;
    UInt16 uInt16Val = 0;
    
	/* 分析inPacketBuffer中的数据,针对不同的域配置相应数据成员的值 */
    while (theFieldP < (theEndP - 2))
    {
        FieldIndex theFieldIndex = kIllegalField;
        UInt32 theFieldLen = 0;
		/* 取前两个字节分析如下,详见QTSS API Doc."流的缓存" */
		/* 注意仅在standard RTP Metal Info中才有Field Name这项 */
        FieldName* theFieldName = (FieldName*)theFieldP;//UInt16
        
		/* 当为压缩格式的RTP元数据时(前两字节的第一个bit为1而非0) */
        if (*theFieldName & 0x8000)
        {
			/* 对入参inFieldIDArray提出要求 */
            Assert(inFieldIDArray != NULL);
            
            // If this is a compressed field, find to which field the ID maps
			/* 取出头两个字节的2-8bit(第一个bit为1),代表是FieldID */
            UInt8 theFieldID = *theFieldP & 0x7F;
             
			/* 查找这个FieldID作为分量在FieldIDArray(所以要求入参inFieldIDArray非空)数组中对应的索引(theFieldIndex),
			   从0到5遍历,注意不能用RTPMetaInfoPacket::FieldIndex RTPMetaInfoPacket::GetFieldIndexForName()一步搞定,
			   因为是FieldID,不是FieldName,但是基本思想是相同的 */
            for (int x = 0; x < kNumFields; x++)
            {
                if (theFieldID == inFieldIDArray[x])
                {
                    theFieldIndex = x;
                    break;
                }
            }
            
			/* 取出第二个字节作为长度FieldLen,注意theFieldP仍是UInt8* */
            theFieldLen = *(theFieldP + 1);
			/* 将指针移到Field Data位置 */
            theFieldP += 2;
        }
        else /* 当为standard(uncompressed)RTP Mata Info数据时 */
        {
            // This is not a compressed field. Make sure there is enough room
            // in the packet for this to make sense
            if (theFieldP >= (theEndP - 4))
                break;

			/* 得到FieldName */
            ::memcpy(&uInt16Val, theFieldP, sizeof(uInt16Val));
			/* 得到FieldName对应的FieldIndex */
            theFieldIndex = this->GetFieldIndexForName(ntohs(uInt16Val));
            
			/* 得到Field length */
            ::memcpy(&uInt16Val, theFieldP + 2, sizeof(uInt16Val));
            theFieldLen = ntohs(uInt16Val);
			/* 将指针移到Field Data位置处 */
            theFieldP += 4;
        }
        
        //
        // Validate the length of this field if possible.
        // If the field is of the wrong length, return an error.
		/* 检查各个域Field length是否是指定的大小(8/2/0字节)? */
        if ((kFieldLengthValidator[theFieldIndex] > 0) &&
            (kFieldLengthValidator[theFieldIndex] != theFieldLen))
            return false;
		/* 检查是否内存越界? */
        if ((theFieldP + theFieldLen) > theEndP)
            return false;
            
        //
        // We now know what field we are dealing with, so store off
        // the proper value depending on the field
		/* 根据不同的Field配置相应数据成员的值,注意theFieldP现在位于Field Data位置处 */
        switch (theFieldIndex)
        {
            case kPacketPosField:
            {
				/* 获取8字节的该Field实际的Data */
                ::memcpy(&sInt64Val, theFieldP, sizeof(sInt64Val));
				/* 经转换处理后得到fPacketPosition */
                fPacketPosition = (UInt64)OS::NetworkToHostSInt64(sInt64Val);
                break;
            }
            case kTransTimeField:
            {
				/* 获取8字节的该Field实际的Data */
                ::memcpy(&sInt64Val, theFieldP, sizeof(sInt64Val));
				/* 经转换处理后得到fTransmitTime */
                fTransmitTime = (UInt64)OS::NetworkToHostSInt64(sInt64Val);
                break;
            }
            case kFrameTypeField:
            {
				/* 不要转换,直接得到fFrameType */
                fFrameType = ntohs(*((FrameTypeField*)theFieldP));
                break;
            }
            case kPacketNumField:
            {
				/* 获取8字节的该Field实际的Data */
                ::memcpy(&sInt64Val, theFieldP, sizeof(sInt64Val));
                fPacketNumber = (UInt64)OS::NetworkToHostSInt64(sInt64Val);
                break;
            }
            case kSeqNumField:
            {
                /* 获取2字节的该Field实际的Data */
                ::memcpy(&uInt16Val, theFieldP, sizeof(uInt16Val));
                fSeqNum = ntohs(uInt16Val);
                break;
            }
            case kMediaDataField:
            {
				/* 直接获得FieldP和它的长度,这两个量前面已做好准备 */
                fMediaDataP = theFieldP;
                fMediaDataLen = theFieldLen;
                break;
            }
            default:
                break;
        }
        
        //
        // Skip onto the next field
		/* 指针移过Field的data部分,来到下一个域的开头 */
        theFieldP += theFieldLen;
    }
    return true;
}

/* 将RTPMetaInfoPacket转变成RTP Packet.试问:RTPMetaInfoPacket和RTP Packet关系怎样? 详见笔记分析 */
UInt8* RTPMetaInfoPacket::MakeRTPPacket(UInt32* outPacketLen)
{
	/* 确保要有media data */
    if (fMediaDataP == NULL)
        return NULL;
    
    //
    // Just move the RTP header to right before the media data.
	/* 将RTP header Info(12字节)移至Media data前面 */
    ::memmove(fMediaDataP - 12, fPacketBuffer, 12);
    
    //
    // Report the length of the resulting RTP packet 
	/* 记录下新生成RTP的位置? */
    if (outPacketLen != NULL)
        *outPacketLen = fMediaDataLen + 12;
    
	/* 返回新的RTP Packet的起始位置 */
    return fMediaDataP - 12;
}


