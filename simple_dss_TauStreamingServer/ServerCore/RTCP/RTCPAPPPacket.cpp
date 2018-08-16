
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTCPAPPPacket.cpp
Description: define a RTCPCompressedQTSSPacket class which extends the RTCP APP 
             packet in the RFC 3550.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 



#include "RTCPAPPPacket.h"
#include "MyAssert.h"
#include "OS.h"
#include "OSMemory.h"



/****** RTCPCompressedQTSSPacket is the packet type that the client actually sends ******/

RTCPCompressedQTSSPacket::RTCPCompressedQTSSPacket(Bool16 debug) :
    RTCPPacket(),
    mDumpArray(NULL),
    fDebug(debug),            //默认不调试
    fRTCPAPPDataBuffer(NULL),
    
    fReceiverBitRate(0),
    fAverageLateMilliseconds(0),
    fPercentPacketsLost(0),
    fAverageBufferDelayMilliseconds(0),
    fIsGettingBetter(false),
    fIsGettingWorse(false),
    fNumEyes(0),
    fNumEyesActive(0),
    fNumEyesPaused(0),
	fOverbufferWindowSize(kUInt32_Max),//默认最大
    
    //Proposed - are these there yet?
    fTotalPacketsReceived(0),
    fTotalPacketsDropped(0),
    fTotalPacketsLost(0),
    fClientBufferFill(0),
    fFrameRate(0),
    fExpectedFrameRate(0),
    fAudioDryCount(0)
{
    if (fDebug)
    {
       mDumpArray = NEW char[1024];
       mDumpArray[0] = '\0'; //首字节置空
       mDumpArrayStrDeleter.Set(mDumpArray); //设置好用完后,自动清除
    }
}

//解析eCompressedQTSSPacket的合法性,若合法时,解析App数据缓存,配置各数据成员的值,并依次放入用于调试输出的字符数组中
Bool16 RTCPCompressedQTSSPacket::ParseCompressedQTSSPacket(UInt8* inPacketBuffer, UInt32 inPacketLength)
{
	//解析RTCP包头是否合法? 若不合法,直接返回false
    if (!this->ParsePacket(inPacketBuffer, inPacketLength))
        return false;
    
	//假如复合包的长度<RTCP包头的长度(8字节)+APP包的长度(12字节),直接返回false
    if (inPacketLength < (kRTCPPacketSizeInBytes + kQTSSDataOffset))
        return false;

    fRTCPAPPDataBuffer = inPacketBuffer+kRTCPPacketSizeInBytes;//得到APP包的缓存起始位置(跳过RTCP包头)

    //figure out how many 32-bit words remain in the buffer,找出除RTCP包头和APP包头,缓存中剩余数据的长度,它是若干个QTSSItem的形式
    UInt32 theMaxDataLen = (inPacketLength - kRTCPPacketSizeInBytes) - kQTSSDataOffset;
    theMaxDataLen /= 4;//是4字节的倍数
    
    //if the number of 32 bit words reported in the packet is greater than the theoretical limit,return an error
    //假如从AppPacketLen域的值(4字节的长度)>缓存中剩余数据的长度(4字节的长度),直接返回false
    if (this->GetAppPacketLength() > theMaxDataLen)
        return false;
    
	//假如AppPacketVersion域的值不为指定值0,直接返回false
    if (this->GetAppPacketVersion() != kSupportedCompressedQTSSVersion)
        return false;
    
	//假如RC域不为0,直接返回false
    if (this->GetReportCount() > 0)
        return false;
     
	//解析App数据缓存,配置各数据成员的值,并依次放入用于调试输出的字符数组中
    this->ParseAndStore();

    return true;
}

//解析App数据缓存,配置各数据成员的值,并依次放入用于调试输出的字符数组mDumpArray中
void RTCPCompressedQTSSPacket::ParseAndStore()
{
#define APPEND_TO_DUMP_ARRAY(f, v) {if (fDebug) (void)qtss_sprintf(&mDumpArray[strlen(mDumpArray)], f, v);}

	//获取相应格式化字符串,依次存入mDumpArray字符数组中
    FourCharCode appName = this->GetAppPacketName();
    APPEND_TO_DUMP_ARRAY("       H_app_packet_name = %.4s, ", (char*)&appName);
    APPEND_TO_DUMP_ARRAY("H_src_ID = %lu, ", this->GetReportSourceID());
    APPEND_TO_DUMP_ARRAY("H_vers=%d, ", this->GetAppPacketVersion());
    APPEND_TO_DUMP_ARRAY("H_packt_len=%d", this->GetAppPacketLength());

    UInt8* qtssDataBuffer = fRTCPAPPDataBuffer+kQTSSDataOffset;//获取DataBuffer的起始地址
    UInt32 bytesRemaining = this->GetAppPacketLength() * 4; //packet length is given in words,获得数据包的长度(不含前面的APP包头)
    
	
	while ( bytesRemaining >= 4 ) //items must be at least 32 bits
    {
        // DMS - There is no guarantee that qtssDataBuffer will be 4 byte aligned, because
        // individual APP packet fields can be 6 bytes or 4 bytes or 8 bytes. So we have to
        // use the 4-byte align protection functions. Sparc and MIPS processors will crash otherwise
		// 使用4字节内存对齐函数OS::GetUInt32FromMemory(),得到QTSSItem项的起始地址
        UInt32 theHeader = ntohl(OS::GetUInt32FromMemory((UInt32*)&qtssDataBuffer[kQTSSItemTypeOffset]));
        UInt16 itemType = (UInt16)((theHeader & kQTSSItemVersionMask) >> kQTSSItemVersionShift);
		UInt8 itemVersion = (UInt8)((theHeader & kQTSSItemTypeMask) >> kQTSSItemTypeShift);
        UInt8 itemLengthInBytes = (UInt8)(theHeader & kQTSSItemLengthMask);

        APPEND_TO_DUMP_ARRAY("\n       H_type=%.2s(", (char*)&itemType);
        APPEND_TO_DUMP_ARRAY("vers=%u", itemVersion);
        APPEND_TO_DUMP_ARRAY(", H_size=%u", itemLengthInBytes);

        qtssDataBuffer += sizeof(UInt32);   //advance past the above UInt16's & UInt8's (point to the actual item data),指向该item的实际数据
        
        //Update bytesRemaining (move it past current item)
        //This itemLengthInBytes is part of the packet and could therefore be bogus.
        //Make sure not to overstep the end of the buffer!
        bytesRemaining -= sizeof(UInt32);
        if (itemLengthInBytes > bytesRemaining)
            break; //don't walk off the end of the buffer
        //跳过该item的实际数据
        bytesRemaining -= itemLengthInBytes;
        
		//下面对item的类型逐一分情形讨论:
        switch (itemType)
        {
            case  TW0_CHARS_TO_INT( 'r', 'r' ): //'rr': //'rrcv': rate receiver
            {
                fReceiverBitRate = ntohl(OS::GetUInt32FromMemory((UInt32*)qtssDataBuffer));
                qtssDataBuffer += sizeof(fReceiverBitRate);
                APPEND_TO_DUMP_ARRAY(", rcvr_bit_rate=%lu", fReceiverBitRate);
            }
            break;
            
            case TW0_CHARS_TO_INT('l', 't'): //'lt':    //'late':
            {
                fAverageLateMilliseconds = ntohs(*(UInt16*)qtssDataBuffer);
                qtssDataBuffer += sizeof(fAverageLateMilliseconds);
                APPEND_TO_DUMP_ARRAY(", avg_late=%u", fAverageLateMilliseconds);
            }
            break;
            
            case TW0_CHARS_TO_INT('l', 's'): // 'ls':   //'loss':
            {
                fPercentPacketsLost = ntohs(*(UInt16*)qtssDataBuffer);
                qtssDataBuffer += sizeof(fPercentPacketsLost);
                APPEND_TO_DUMP_ARRAY(", percent_loss=%u", fPercentPacketsLost);
            }
            break;
            
            case TW0_CHARS_TO_INT('d', 'l'): //'dl':    //'bdly': buffer delay
            {
                fAverageBufferDelayMilliseconds = ntohs(*(UInt16*)qtssDataBuffer);
                qtssDataBuffer += sizeof(fAverageBufferDelayMilliseconds);
                APPEND_TO_DUMP_ARRAY(", avg_buf_delay=%u", fAverageBufferDelayMilliseconds);
            }
            break;
            
            case TW0_CHARS_TO_INT(':', ')' ): //':)':   //':|:(':
            {
                fIsGettingBetter = true;
                APPEND_TO_DUMP_ARRAY(", :|:(=%s","yes");
            }
            break;
            
            case TW0_CHARS_TO_INT(':', '(' ): // ':(':  //':|:)':
            {
                fIsGettingWorse = true;
                APPEND_TO_DUMP_ARRAY(", :|:)=%s","yes");
            }
            break;
            
            case TW0_CHARS_TO_INT('e', 'y' ): //'ey':   //'eyes':
            {
                fNumEyes = ntohl(OS::GetUInt32FromMemory((UInt32*)qtssDataBuffer));
                qtssDataBuffer += sizeof(fNumEyes);             
                APPEND_TO_DUMP_ARRAY(", eyes=%lu", fNumEyes);

                if (itemLengthInBytes >= 2)
                {
                    fNumEyesActive = ntohl(OS::GetUInt32FromMemory((UInt32*)qtssDataBuffer));
                    qtssDataBuffer += sizeof(fNumEyesActive);
                    APPEND_TO_DUMP_ARRAY(", eyes_actv=%lu", fNumEyesActive);
                }
                if (itemLengthInBytes >= 3)
                {
                    fNumEyesPaused = ntohl(OS::GetUInt32FromMemory((UInt32*)qtssDataBuffer));
                    qtssDataBuffer += sizeof(fNumEyesPaused);
                    APPEND_TO_DUMP_ARRAY(", eyes_pausd=%lu", fNumEyesPaused);
                }
            }
            break;
            
            case TW0_CHARS_TO_INT('p', 'r' ): // 'pr':  //'prcv':
            {
                fTotalPacketsReceived = ntohl(OS::GetUInt32FromMemory((UInt32*)qtssDataBuffer));
                qtssDataBuffer += sizeof(fTotalPacketsReceived);
                APPEND_TO_DUMP_ARRAY(", pckts_rcvd=%lu", fTotalPacketsReceived);
            }
            break;
            
            case TW0_CHARS_TO_INT('p', 'd'): //'pd':    //'pdrp':
            {
                fTotalPacketsDropped = ntohs(*(UInt16*)qtssDataBuffer);
                qtssDataBuffer += sizeof(fTotalPacketsDropped);
                APPEND_TO_DUMP_ARRAY(", pckts_drppd=%u", fTotalPacketsDropped);
            }
            break;
            
            case TW0_CHARS_TO_INT('p', 'l'): //'pl':    //'p???':
            {
                fTotalPacketsLost = ntohs(*(UInt16*)qtssDataBuffer);
                qtssDataBuffer += sizeof(fTotalPacketsLost);
                APPEND_TO_DUMP_ARRAY(", ttl_pckts_lost=%u", fTotalPacketsLost);
            }
            break;
            
            
            case TW0_CHARS_TO_INT('b', 'l'): //'bl':    //'bufl':
            {
                fClientBufferFill = ntohs(*(UInt16*)qtssDataBuffer);
                qtssDataBuffer += sizeof(fClientBufferFill);
                APPEND_TO_DUMP_ARRAY(", buffr_fill=%u", fClientBufferFill);
            }
            break;
            
            
            case TW0_CHARS_TO_INT('f', 'r'): //'fr':    //'frat':
            {
                fFrameRate = ntohs(*(UInt16*)qtssDataBuffer);
                qtssDataBuffer += sizeof(fFrameRate);
                APPEND_TO_DUMP_ARRAY(", frame_rate=%u", fFrameRate);
            }
            break;
            
            
            case TW0_CHARS_TO_INT('x', 'r'): //'xr':    //'xrat':
            {
                fExpectedFrameRate = ntohs(*(UInt16*)qtssDataBuffer);
                qtssDataBuffer += sizeof(fExpectedFrameRate);
                APPEND_TO_DUMP_ARRAY(", xpectd_frame_rate=%u", fExpectedFrameRate);
            }
            break;
            
            
            case TW0_CHARS_TO_INT('d', '#'): //'d#':    //'dry#':
            {
                fAudioDryCount = ntohs(*(UInt16*)qtssDataBuffer);
                qtssDataBuffer += sizeof(fAudioDryCount);
                APPEND_TO_DUMP_ARRAY(", aud_dry_count=%u", fAudioDryCount);
            }
            break;
            
            case TW0_CHARS_TO_INT('o', 'b'): //'ob': // overbuffer window size
            {
                fOverbufferWindowSize = ntohl(OS::GetUInt32FromMemory((UInt32*)qtssDataBuffer));
                qtssDataBuffer += sizeof(fOverbufferWindowSize);
                APPEND_TO_DUMP_ARRAY(", ovr_buffr_windw_siz=%lu", fOverbufferWindowSize);
            }
            break;
            
            default:
            {
                if (fDebug)
				{
				   char s[12] = "";
                   qtss_sprintf(s, "  [%.2s]", (char*)&itemType);
                   WarnV(false, "Unknown APP('QTSS') item type");
                   WarnV(false, s);
            	}
	        }

            break;
        }   //switch (itemType)

        
        APPEND_TO_DUMP_ARRAY("%s", "),  ");

    }   //while ( bytesRemaining >= 4 )

}

//先打印出RTCP包头,再打印出存贮在mDumpArray中的所有数据
void RTCPCompressedQTSSPacket::Dump()//Override
{
    RTCPPacket::Dump();

    qtss_printf("%s \n", mDumpArray);
}



