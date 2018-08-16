
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTCPSRPacket.cpp
Description: implement a compound RTCP packet including SR,SDES,BYE and Server info packets
             which sent by the Server side.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 

#include <string.h>

#include "RTCPSRPacket.h"
#include "MyAssert.h"
#include "OS.h"


//构造函数,注意数据成员的赋值,十分重要
RTCPSRPacket::RTCPSRPacket()
{
    // Write as much of the Sender Report as is possible
    char theTempCName[kMaxCNameLen];
    UInt32 cNameLen = RTCPSRPacket::GetACName(theTempCName);//得到一个cName
    
    //write the SR & SDES headers
    UInt32* theSRWriter = (UInt32*)&fSenderReportBuffer;
    *theSRWriter = htonl(0x80c80006);//写SR的包头
    theSRWriter += 7; //number of UInt32s in an SR.
    
    //SDES length is the length of the CName, plus 2 32bit words (one for packet header, the other for the SSRC)
    *theSRWriter = htonl(0x81ca0000 + (cNameLen >> 2) + 1);//写SDES包的包头,数据长度为变长,依CName长度定,再加1
    ::memcpy(&fSenderReportBuffer[kSenderReportSizeInBytes], theTempCName, cNameLen);
    fSenderReportSize = kSenderReportSizeInBytes + cNameLen;

/*
 SERVER INFO PACKET FORMAT
struct qtss_rtcp_struct
{
    RTCPHeader      header;
    UInt32          ssrc;       // ssrc of rtcp originator
    OSType          name;
    UInt32          senderSSRC;
    SInt16          reserved;
    SInt16          length;     // bytes of data (atoms) / 4
    // qtsi_rtcp_atom structures follow
};
*/

    // Write the SERVER INFO APP packet
    UInt32* theAckInfoWriter = (UInt32*)&fSenderReportBuffer[fSenderReportSize];
    *theAckInfoWriter = htonl(0x81cc0006);
    theAckInfoWriter += 2;
    *(theAckInfoWriter++) = htonl(FOUR_CHARS_TO_INT('q', 't', 's', 'i')); // Ack Info APP name
    theAckInfoWriter++; // leave space for the ssrc (again)
    *(theAckInfoWriter++) = htonl(2); // 2 UInt32s for the 'at' field
    *(theAckInfoWriter++) = htonl(FOUR_CHARS_TO_INT( 'a', 't', 0, 4 ));
    fSenderReportWithServerInfoSize = (char*)(theAckInfoWriter+1) - fSenderReportBuffer;    
    
	// Write BYE packet
    UInt32* theByeWriter = (UInt32*)&fSenderReportBuffer[fSenderReportWithServerInfoSize];
    *theByeWriter = htonl(0x81cb0001);
}

//对给定的入参,得到一个cName,并返回它的真实数据的长度(含padding bit)
UInt32 RTCPSRPacket::GetACName(char* ioCNameBuffer)
{
    static char*    sCNameBase = "QTSS";

    //clear out the whole buffer 初始化入参
    ::memset(ioCNameBuffer, 0, kMaxCNameLen);
    
    //cName identifier
    ioCNameBuffer[0] = 1;
    
    //Unique cname is constructed from the base name and the current time
    qtss_sprintf(&ioCNameBuffer[2], " %s%"_64BITARG_"d", sCNameBase, OS::Milliseconds() / 1000); //1应为2?
    UInt32 cNameLen = ::strlen(ioCNameBuffer);
    //2nd byte of CName should be length
    ioCNameBuffer[1] = (UInt8) (cNameLen - 2);//don't count indicator or length byte,不要计算前两个字节(indicator和length位)

    // This function assumes that the cName is the only item in this SDES chunk (see RTP rfc 3550 for details). 
    // The RFC says that the item (the cName) should not be NULL terminated, but
    // the chunk *must* be NULL terminated. And padded to a 32-bit boundary.
    // qtss_sprintf already put a NULL terminator in the cName buffer. So all we have to
    // do is pad out to the boundary.
    cNameLen += 1; //add on the NULL character
    UInt32 paddedLength = cNameLen + (4 - (cNameLen % 4));//再附加几bit凑成4字节的倍数,先统计附加的长度
    
    // Pad, and zero out as we pad.附加几个值为0的比特
    for (; cNameLen < paddedLength; cNameLen++)
        ioCNameBuffer[cNameLen] = '\0';
    
    Assert((cNameLen % 4) == 0);
    return cNameLen;
}

/*
SDES: Source Description
------------------------
0             1               2               3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P| SC    | PT=SDES=202   |              length             | header 
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                        SSRC/CSRC_1                            | chunk 
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        SDES items                             |
|                            ...                                |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                        SSRC/CSRC_2                            | chunk 2 
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        SDES items                             |
|                            ...                                |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

Sender Report
---------------
		0                   1                   2                   3
		0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
header  |V=2|P|    RC   |   PT=SR=200   |             length            |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                         SSRC of sender                        |
		+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
sender  |              NTP timestamp, most significant word             |
info    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|             NTP timestamp, least significant word             |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                         RTP timestamp                         |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                     sender's packet count                     |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                      sender's octet count                     |
		+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
report  |                 SSRC_1 (SSRC of first source)                 |
block   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
1       | fraction lost |       cumulative number of packets lost       |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|           extended highest sequence number received           |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                      interarrival jitter                      |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                         last SR (LSR)                         |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                   delay since last SR (DLSR)                  |
		+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
report  |                 SSRC_2 (SSRC of second source)                |
block   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
2    :                               ...                             :
		+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
		|                  profile-specific extensions                  |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

		APP: Application-defined RTCP packet

		0                   1                   2                   3
		0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|V=2|P| subtype |   PT=APP=204  |             length            |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                           SSRC/CSRC                           |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                          name (ASCII)                         |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                   application-dependent data                  |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

*/