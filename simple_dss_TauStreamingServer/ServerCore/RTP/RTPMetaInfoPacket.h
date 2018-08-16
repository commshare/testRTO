
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPMetaInfoPacket.h
Description: Some defs for RTP-Meta-Info payloads. This class also parses RTP meta info packets.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef __QTRTP_META_INFO_PACKET_H__
#define __QTRTP_META_INFO_PACKET_H__

#include <stdlib.h>
#include "SafeStdLib.h"
#include "OSHeaders.h"
#include "StrPtrLen.h"

class RTPMetaInfoPacket
{
    public:
    
        //
        // RTP Meta Info Fields
        
		/* the following fields occupy 2 bytes each */
		/* 下面是FieldIndex的定义 */
        enum
        {
            kPacketPosField         = 0, //TW0_CHARS_TO_INT('p', 'p'),
            kTransTimeField         = 1, //TW0_CHARS_TO_INT('t', 't'),
            kFrameTypeField         = 2, //TW0_CHARS_TO_INT('f', 't'),
            kPacketNumField         = 3, //TW0_CHARS_TO_INT('p', 'n'),
            kSeqNumField            = 4, //TW0_CHARS_TO_INT('s', 'q'),
            kMediaDataField         = 5, //TW0_CHARS_TO_INT('m', 'd'),
            
            kIllegalField           = 6,
            kNumFields              = 6
        };
        typedef UInt16 FieldIndex;
        
        //
        // Types
        
        typedef UInt16 FieldName;
        typedef SInt32 FieldID; /* cited by QTHintTrack.h,QTSSModuleUtils::AppendRTPMetaInfoHeader() */
        
        //
        // Special field IDs
		/* 参见QTSSModuleUtils::AppendRTPMetaInfoHeader(),表示FieldIDArray的分量值 */
        enum
        {
			/* 在存在FieldID时,可以不用ParsePacket()中的方法判断这个Field是否被compressed,参见QTHintTrack::WriteMetaInfoField() */
            kUncompressed = -1,     // No field ID (not a compressed field) in a RTP Meta Info field 
            kFieldNotUsed = -2      // This field is not being used,this is the initial value of Field ID Array
        };
        
        //
        // Routine that converts the above enum items into real field names
        static FieldName GetFieldNameForIndex(FieldIndex inIndex) { return kFieldNameMap[inIndex]; }
        static FieldIndex GetFieldIndexForName(FieldName inName);
        
        //
        // Routine that constructs a standard FieldID Array out of a x-RTP-Meta-Info header
		/* 参见QTSSModuleUtils::AppendRTPMetaInfoHeader() */
		/* 从入参RTP净负荷元信息的报头中提取相关数据创建FieldIDArray */
        static void ConstructFieldIDArrayFromHeader(StrPtrLen* inHeader, FieldID* ioFieldIDArray);
        
        //
        // Values for the Frame Type Field  
        enum
        {
            kUnknownFrameType   = 0,
            kKeyFrameType       = 1, /* this we want */
            kBFrameType         = 2,
            kPFrameType         = 3
        };
        typedef UInt16 FrameTypeField; /* very important */
        
        
        //
        // CONSTRUCTOR
        
        RTPMetaInfoPacket() :   fPacketBuffer(NULL),
                                fPacketLen(0),
                                fTransmitTime(0),
                                fFrameType(kUnknownFrameType),
                                fPacketNumber(0),
                                fPacketPosition(0),
                                fMediaDataP(NULL),
                                fMediaDataLen(0),
                                fSeqNum(0)          {}
        ~RTPMetaInfoPacket() {}
        
        //
        // Call this to parse the RTP-Meta-Info packet.
        // Pass in an array of FieldIDs, make sure it is kNumFields(see above) in length.
        // This function will use the array as a guide to tell which field IDs in the
        // packet refer to which fields.
		/* 分析入参inPacketBuffer中的数据,解析各Field的值,针对不同的域配置相应数据成员的值 */
        Bool16  ParsePacket(UInt8* inPacketBuffer, UInt32 inPacketLen, FieldID* inFieldIDArray);
        
        //
        // Call this if you would like to rewrite the Meta-Info packet
        // as a normal RTP packet (strip off the extensions). Note that
        // this will overwrite data in the buffer!
        // Returns a pointer to the new RTP packet, and its length
        UInt8*          MakeRTPPacket(UInt32* outPacketLen);
        
        //
        // Field Accessors
        SInt64          GetTransmitTime()       { return fTransmitTime; }
        FrameTypeField  GetFrameType()          { return fFrameType; }   /* very important */
        UInt64          GetPacketNumber()       { return fPacketNumber; }
        UInt64          GetPacketPosition()     { return fPacketPosition; }
        UInt8*          GetMediaDataP()         { return fMediaDataP; }
        UInt32          GetMediaDataLen()       { return fMediaDataLen; }
        UInt16          GetSeqNum()             { return fSeqNum; }
    
    private:
    
		/* RTP Meta Info Packet的缓存起点和长度,注意比RTP Packet大并包含RTP Packet */
        UInt8*          fPacketBuffer;
        UInt32          fPacketLen;
        
		/* 6 important elements in RTPMataInfo packet */
        SInt64          fTransmitTime;
        FrameTypeField  fFrameType; /* this we really want */
        UInt64          fPacketNumber;
        UInt64          fPacketPosition;

		/* 下面这两个算一个 */
        UInt8*          fMediaDataP; /* pointer to media data */
        UInt32          fMediaDataLen;

        UInt16          fSeqNum;
        
		/* the following 常量 are defined *.cpp */
		/* 注意还有一个就是 FieldID* kFieldIDArray,这个多作为函数参数 */
        static const FieldName kFieldNameMap[];
        static const UInt32 kFieldLengthValidator[];
};

#endif // __QTRTP_META_INFO_PACKET_H__
