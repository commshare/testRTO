
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 SDPUtils.h
Description: define some static routines for dealing with SDPs.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 

#ifndef __SDPUtilsH__
#define __SDPUtilsH__

#include "OS.h"
#include "StrPtrLen.h"
#include "ResizeableStringFormatter.h"
#include "StringParser.h"
#include "OSMemory.h"

/* become each sdp line into a common C-String with 0-terminated */
/* 注意SDPLine由每个SDP的第一个字符组成,是个字符的集合 */
class SDPLine : public StrPtrLen //StrPtrLen是基类
{
public:
	/* Initialize leading header type with '\0' */
	SDPLine() : fHeaderType('\0') {}//子类构造函数并初始化
    virtual ~SDPLine() {}

	/* file Header type */
	/* 每一行开头的第一个字符是标志位,非常重要! */
    char    fHeaderType;
};

class SDPContainer
{
	/* constants */

	/* assume that SDP file has 20 lines and  have max 256 characters in each lines */
    enum { kBaseLines = 20, kLineTypeArraySize = 256};

	/* give v,s,t,o s' position offset */
    enum {  
            kVPos = 0, /* version */
            kSPos,     /* session name */
            kTPos,     /* time */
            kOPos      /* original */
         };

	/* 这四个行是必须要求的! */
    enum {
            kV = 1 << kVPos, 
            kS = 1 << kSPos,
            kT = 1 << kTPos,
            kO = 1 << kOPos,
            kAllReq = kV | kS | kT | kO
          };
            

public:
    /* initialize constructor with default max number of sdp lines */
    SDPContainer(UInt32 numStrPtrs = SDPContainer::kBaseLines) : //构造函数,have 20 string pointers
        fNumSDPLines(numStrPtrs), //numStrPtrs是形参
        fSDPLineArray(NULL)
    {   
        Initialize();//定义见下面，返回空指针
    }

    ~SDPContainer() {delete [] fSDPLineArray;}//析构函数

	/* MEMBER FUNCTIONS WHICH HANDLE SDP LINES SIMPLY AND EASILY */

	/* def see SDPUtils.cpp */
	void		Initialize();

	/* add sdp header type and line  which uses the StrPtrLen class pointer */
	/* add the given line to the next line of the current one and return this line number */
    SInt32      AddHeaderLine (StrPtrLen *theLinePtr); //StrPtrLen是inherited 基类

	/* find out the type of sdp header line */
	/* find out which line  the sdp line with the given id as fHeaderType locates and return the line index*/
	/* 从指定行号start开始搜索与id相匹配的HeaderLineType,返回它所在的行号 */
    SInt32      FindHeaderLineType(char id, SInt32 start);
	/* get fHeaderType of the next line */
    SDPLine*    GetNextLine();
	/* get fHeaderType of the line with the designated line index */
	/* used in SDPUtils::SDPLineSorter() */
    SDPLine*    GetLine(SInt32 lineIndex);
	/* set fHeaderType of the line with the given index */
    void        SetLine(SInt32 index);

	/* FUNDAMENTAL FUNCTION */
	/* 逐行解析fSDPBuffer中sdp数据的合法性,将合法的数据加到当前行的下一步,最后设定合法行结论fValid */
    void        Parse();

	/* set SDP buffer size */
    Bool16      SetSDPBuffer(char *sdpBuffer);
	/* 对fSDPBuffer中的sdp数据先initialize(),再Parse(),最后返回fValid */
    Bool16      SetSDPBuffer(StrPtrLen *sdpBufferPtr);//StrPtrLen是基类
	/* judge buffer validity */
	/* 判断sdp数据在buffer中存放是否是合法的形式,需要先用SDPContainer::Parse()判断  */
    Bool16      IsSDPBufferValid() {return fValid;}

	/* has request line ? */
    Bool16      HasReqLines() { return (Bool16) (fReqLines == kAllReq) ; } //fReqLines定义见下面，kAllReq定义见上面
	/* 判断给定的lineType是否在fFieldStr中? */
    Bool16      HasLineType( char lineType ) { return (Bool16) (lineType == fFieldStr[lineType]) ; }//fFieldStr数组定义见下
    char*       GetReqLinesArray;

	/* print sdp lines */
    void        PrintLine(SInt32 lineIndex);
    void        PrintAllLines();

	/* return the number of the actual used sdp  line */
    SInt32      GetNumLines() { return  fNumUsedLines; }//fNumUsedLines定义见下
    
	/*********************** member variables  ***************************/

    SInt32      fCurrentLine; //current line # in sdp file
    SInt32      fNumSDPLines; //total number of lines in sdp file
    SInt32      fNumUsedLines; //total number of the actual used sdp  line,实际使用的sdp行数,证明见AddHeaderLine()
	/******指向每个sdp的行的第一个字符或者理解为由每个sdp行第一个字符组成的字符数组************/
    SDPLine*    fSDPLineArray; //pointer to array consists of the first char of the lines in sdp file, very important!
    Bool16      fValid;//enormous valid state of each sdp line in buffer, see SDPContainer::Parse()
	/******************  实际存放的是SDP数据  ***************************************/
    StrPtrLen   fSDPBuffer;// sdp buffer which we really counter and is input stream
    UInt16      fReqLines; //the request sdp line,它含有'vosi'吗?

    /* char array consists of fHeaderType in each sdp line with the default max size 256 */
	/* 这些标识字符以它们在ASCII码表中的位置出现 */
    char        fFieldStr[kLineTypeArraySize]; // 域名数组，引用见上

};

/** used in QTSSFileModule::DoDescribe(), two functions SDPLineSorter() and GetSortedSDPCopy() see SDPUtils.cpp */
class SDPLineSorter {

public:
	SDPLineSorter(): fSessionLineCount(0),fSDPSessionHeaders(NULL,0), fSDPMediaHeaders(NULL,0) {};//构造函数
	/*************************重量级函数*****************************************************************/
	SDPLineSorter(SDPContainer *rawSDPContainerPtr, Float32 adjustMediaBandwidthPercent = 1.0);//另一个构造函数
	
	/* obtain Session and Media headers */
	StrPtrLen* GetSessionHeaders() { return &fSessionHeaders; }//fSessionHeaders定义见下
	StrPtrLen* GetMediaHeaders() { return &fMediaHeaders; } //fMediaHeaders定义见下

	/* get sorted sdp line copy */
	char* GetSortedSDPCopy();
	
	/*****************************************************************************************************/
	//
	// member variables
	//
	/* sdp line buffer, SPL代表StrPtrLen */
	StrPtrLen fullSDPBuffSPL;

	/* total number of sdp session-level line */
	/* 找到SDP行的行号(从0开始) */
	SInt32 fSessionLineCount;

	/* sdp container with a sdp session  */
	/* 注意这个量将class SDPContainer联系起来  */
	SDPContainer fSessionSDPContainer;

	/* sdp session and media header  */
	/* 以sSessionOrderedLines[]= "vosiuepcbtrzka"为固定顺序,将以相同的字母开头的行放在一起,形成fSDPSessionHeaders */
	/* 参见SDPLineSorter(SDPContainer *,Float32)  */
	ResizeableStringFormatter fSDPSessionHeaders;
	/* 从m开头的行以后找到可能的b开头的行,调整其MediaBandwidth,并初始化成员变量fMediaHeaders */
	ResizeableStringFormatter fSDPMediaHeaders;

	/* session and media header  */

	/******************************************************************************/
	//这两个量非常重要.它们组成一个SDP文件.
	/* 将fSDPSessionHeaders最后的结果设置为fSessionHeaders */
	/* 参见SDPLineSorter(SDPContainer *,Float32)  */
	StrPtrLen fSessionHeaders; //fSessionHeaders定义
	/*****用fSDPMediaHeaders的一行初始化另一成员变量fMediaHeaders,找出以m开头的行,以后的行可能包含b开头的行*****/
	StrPtrLen fMediaHeaders;//fMediaHeaders定义，引用见上面

    /* array of fixed-orded sdp session line */
	/* 另见SDPContainer::Parse() */
	static char sSessionOrderedLines[];// = "vosiuepcbtrzka"; // chars are order dependent: declared by rfc 2327
	static char sessionSingleLines[];//  = "vosiuepcbzk";    // return only 1 of each of these session field types
	
	/* the following  both see SDPUtils.cpp */
	/* spd line stop char */
	static StrPtrLen sEOL;
	/* max bandwidth */
    static StrPtrLen sMaxBandwidthTag;
};


#endif

