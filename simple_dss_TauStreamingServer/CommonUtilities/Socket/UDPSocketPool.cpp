/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 UDPSocketPool.cpp
Description: Object that creates & maintains UDP socket pairs in a pool.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#include "UDPSocketPool.h"


/* 对源IP地址和端口分情形: 当至少一个非零时,通过循环查找出符合要求的不在demuxer中的UDPSocketPair;否则新建符合要求的UDPSocketPair */
UDPSocketPair* UDPSocketPool::GetUDPSocketPair(UInt32 inIPAddr, UInt16 inPort,
                                                UInt32 inSrcIPAddr, UInt16 inSrcPort)
{
    OSMutexLocker locker(&fMutex);
	/* 当source IP address or port至少一个存在时,遍历UDPSocketPair组成的pool */
    if ((inSrcIPAddr != 0) || (inSrcPort != 0))
    {
        for (OSQueueIter qIter(&fUDPQueue); !qIter.IsDone(); qIter.Next())
        {
            //If we find a pair that is a) on the right IP address, and b) doesn't
            //have this source IP & port in the demuxer already, we can return this pair
			/* 获取当前UDPSocketPair队列中的Queue elem所在的类对象指针,参见UDPSocketPair类定义 */
            UDPSocketPair* theElem = (UDPSocketPair*)qIter.GetCurrent()->GetEnclosingObject();
			/* 假如UDPSocketPair的本地地址就是入参指定的地址,并且端口要么为0(不关心最后绑定的端口号),要么就是入参指定的端口号 */
            if ((theElem->fSocketA->GetLocalAddr() == inIPAddr) &&
                ((inPort == 0) || (theElem->fSocketA->GetLocalPort() == inPort)))
            {
                //check to make sure this source IP & port is not already in the demuxer.
                //If not, we can return this socket pair.
				/* 假如没有得到UDPSocketPair中另一UDP Socket(RTCP)的UDPDemuxer类对象指针(没有复用),或者找到这个类指针,但是
				   在指定IP地址和端口没有分配Task,就增加计数,并返回 */
                if ((theElem->fSocketB->GetDemuxer() == NULL) ||
                    ((!theElem->fSocketB->GetDemuxer()->AddrInMap(0, 0)) &&
                    (!theElem->fSocketB->GetDemuxer()->AddrInMap(inSrcIPAddr, inSrcPort))))
                {
                    theElem->fRefCount++;
                    return theElem;
                }
                //If port is specified, there is NO WAY a socket pair can exist that matches
                //the criteria (because caller wants a specific ip & port combination)
				/* 假如入参inPort非零,只有一次机会查找,若到这里,意味着查找失败,返回NULL */
                else if (inPort != 0)
                    return NULL;
            }
        }
    }
    //if we get here, there is no UDP Socket pair already in the pool that matches the specified criteria, so we have to create a new pair.
    //假如我们到此,说明当前UDPSocketPair队列中没有符合指定标准的UDPSocketPair,我们只得新建一个,放入该pool中
    return this->CreateUDPSocketPair(inIPAddr, inPort);
}

/* 注意原理类似Critical_Section,减少UDPSocketPair引用计数.考虑到复用器的情形,只有当引用计数为0时,才从队列中删去队列元素,并销毁UDPSocketPair对象实例 */
void UDPSocketPool::ReleaseUDPSocketPair(UDPSocketPair* inPair)
{
    OSMutexLocker locker(&fMutex);
    inPair->fRefCount--;
    if (inPair->fRefCount == 0)
    {
        fUDPQueue.Remove(&inPair->fElem);
        this->DestructUDPSocketPair(inPair);
    }
}

/* 构造UDP Socket Pair,并绑定到指定的IP地址和端口.当输入端口非零时,只有一次绑定成功机会;当输入端口为零时,
   可循环查找可以绑定上的端口.最后返回绑定后的UDP Socket Pair,否则返回NULL */
UDPSocketPair*  UDPSocketPool::CreateUDPSocketPair(UInt32 inAddr, UInt16 inPort)
{
    OSMutexLocker locker(&fMutex);
	/* 由派生类RTPSocketPool(参见QTSServer.cpp)生成UDP Socket Pair,即从服务器获取RTCP任务实例指针并相应创建一个UDPSocketPair实例 */
    UDPSocketPair* theElem = ConstructUDPSocketPair();
	/* 确保生成成功 */
    Assert(theElem != NULL);
	/* 确保UDP Socket Pair两端的UDPSocket都可成功打开,否则销毁它 */
    if (theElem->fSocketA->Open() != OS_NoErr)
    {
		/* 源码实现参见派生类RTPSocketPool(参见QTSServer.cpp),删去入参指定(比如,上面创建)的一个UDPSocketPair实例 */
        this->DestructUDPSocketPair(theElem);
        return NULL;
    }
    if (theElem->fSocketB->Open() != OS_NoErr)
    {
        this->DestructUDPSocketPair(theElem);
        return NULL;
    }
    
    // Set socket options on these new sockets
	/* 由派生类RTPSocketPool(参见QTSServer.cpp)设置UDP Socket Pair的options,即: 依次设置UDP socket对的发送buffer或接收缓存大小:
	第一个设置固定,第二个设置要灵活,即:设置UDPSocketPair中向外发送RTP数据的socket buffer大小为256K字节,从预设值开始,以减半策略
	来动态调整RTCP socket接受buffer的大小 */
    this->SetUDPSocketOptions(theElem);
    
    //try to find an open pair of ports to bind these sockets to
	/* 找一对未使用的端口(范围在6970~65536)来绑定到UDPSocketPair上,初始值设为false,下面设置好后会更改它的值 */
    Bool16 foundPair = false;
    
    //If port is 0, then the caller doesn't care what port # we bind this socket to.
    //Otherwise, ONLY attempt to bind this socket to the specified port
	/* 假如入参指定的端口号为0,则调用者并不关心实际绑定到哪个端口号上;否则,只能绑定入参指定的端口号 */
    UInt16 startIndex = kLowestUDPPort;
    if (inPort != 0)
        startIndex = inPort;
    UInt16 stopIndex = kHighestUDPPort;
    if (inPort != 0)
        stopIndex = inPort;
    
	/* 当要找这一对端口,并且起始端口不超过最大值时,这是循环查找端口的条件 */
    while ((!foundPair) && (startIndex <= kHighestUDPPort))
    {
        OS_Error theErr = theElem->fSocketA->Bind(inAddr, startIndex);
		//当绑定成功RTP端口号后, 再接着绑定RTCP端口号
        if (theErr == OS_NoErr)
        {
            theErr = theElem->fSocketB->Bind(inAddr, startIndex+1);
			/* 当两个Socket都成功绑定时 */
            if (theErr == OS_NoErr)
            {
				/* 显示找到了这两个端口 */
                foundPair = true;
				/* 将绑定后的UDP Socket Pair加入UDPSocketPair的队列 */
                fUDPQueue.EnQueue(&theElem->fElem);
				/* 该UDP Socket Pair计数增加1 */
                theElem->fRefCount++;
				/* 返回该UDP Socket Pair */
                return theElem;
            }
        }
        //If we are looking to bind to a specific port set, and we couldn't then
        //just break it out here.
		/* 假如此时入参inPort非零(说明是不能绑定到指定端口),在上面唯一一次绑定机会中没有成功,就中断 */
        if (inPort != 0)
            break;
		/* 增大两个,再次循环查找端口,注意这里是成对查找,所以是增加2而非1 ! */
        startIndex += 2;
    }
    //if we couldn't find a pair of sockets, make sure to clean up our mess
	/* 对入参指定的端口号为0情形,如果在上面的循环查找中没有成功,就销毁该UDP Socket Pair */
    this->DestructUDPSocketPair(theElem);
    return NULL;
}
