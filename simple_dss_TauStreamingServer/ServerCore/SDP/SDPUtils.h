
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
/* ע��SDPLine��ÿ��SDP�ĵ�һ���ַ����,�Ǹ��ַ��ļ��� */
class SDPLine : public StrPtrLen //StrPtrLen�ǻ���
{
public:
	/* Initialize leading header type with '\0' */
	SDPLine() : fHeaderType('\0') {}//���๹�캯������ʼ��
    virtual ~SDPLine() {}

	/* file Header type */
	/* ÿһ�п�ͷ�ĵ�һ���ַ��Ǳ�־λ,�ǳ���Ҫ! */
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

	/* ���ĸ����Ǳ���Ҫ���! */
    enum {
            kV = 1 << kVPos, 
            kS = 1 << kSPos,
            kT = 1 << kTPos,
            kO = 1 << kOPos,
            kAllReq = kV | kS | kT | kO
          };
            

public:
    /* initialize constructor with default max number of sdp lines */
    SDPContainer(UInt32 numStrPtrs = SDPContainer::kBaseLines) : //���캯��,have 20 string pointers
        fNumSDPLines(numStrPtrs), //numStrPtrs���β�
        fSDPLineArray(NULL)
    {   
        Initialize();//��������棬���ؿ�ָ��
    }

    ~SDPContainer() {delete [] fSDPLineArray;}//��������

	/* MEMBER FUNCTIONS WHICH HANDLE SDP LINES SIMPLY AND EASILY */

	/* def see SDPUtils.cpp */
	void		Initialize();

	/* add sdp header type and line  which uses the StrPtrLen class pointer */
	/* add the given line to the next line of the current one and return this line number */
    SInt32      AddHeaderLine (StrPtrLen *theLinePtr); //StrPtrLen��inherited ����

	/* find out the type of sdp header line */
	/* find out which line  the sdp line with the given id as fHeaderType locates and return the line index*/
	/* ��ָ���к�start��ʼ������id��ƥ���HeaderLineType,���������ڵ��к� */
    SInt32      FindHeaderLineType(char id, SInt32 start);
	/* get fHeaderType of the next line */
    SDPLine*    GetNextLine();
	/* get fHeaderType of the line with the designated line index */
	/* used in SDPUtils::SDPLineSorter() */
    SDPLine*    GetLine(SInt32 lineIndex);
	/* set fHeaderType of the line with the given index */
    void        SetLine(SInt32 index);

	/* FUNDAMENTAL FUNCTION */
	/* ���н���fSDPBuffer��sdp���ݵĺϷ���,���Ϸ������ݼӵ���ǰ�е���һ��,����趨�Ϸ��н���fValid */
    void        Parse();

	/* set SDP buffer size */
    Bool16      SetSDPBuffer(char *sdpBuffer);
	/* ��fSDPBuffer�е�sdp������initialize(),��Parse(),��󷵻�fValid */
    Bool16      SetSDPBuffer(StrPtrLen *sdpBufferPtr);//StrPtrLen�ǻ���
	/* judge buffer validity */
	/* �ж�sdp������buffer�д���Ƿ��ǺϷ�����ʽ,��Ҫ����SDPContainer::Parse()�ж�  */
    Bool16      IsSDPBufferValid() {return fValid;}

	/* has request line ? */
    Bool16      HasReqLines() { return (Bool16) (fReqLines == kAllReq) ; } //fReqLines��������棬kAllReq���������
	/* �жϸ�����lineType�Ƿ���fFieldStr��? */
    Bool16      HasLineType( char lineType ) { return (Bool16) (lineType == fFieldStr[lineType]) ; }//fFieldStr���鶨�����
    char*       GetReqLinesArray;

	/* print sdp lines */
    void        PrintLine(SInt32 lineIndex);
    void        PrintAllLines();

	/* return the number of the actual used sdp  line */
    SInt32      GetNumLines() { return  fNumUsedLines; }//fNumUsedLines�������
    
	/*********************** member variables  ***************************/

    SInt32      fCurrentLine; //current line # in sdp file
    SInt32      fNumSDPLines; //total number of lines in sdp file
    SInt32      fNumUsedLines; //total number of the actual used sdp  line,ʵ��ʹ�õ�sdp����,֤����AddHeaderLine()
	/******ָ��ÿ��sdp���еĵ�һ���ַ��������Ϊ��ÿ��sdp�е�һ���ַ���ɵ��ַ�����************/
    SDPLine*    fSDPLineArray; //pointer to array consists of the first char of the lines in sdp file, very important!
    Bool16      fValid;//enormous valid state of each sdp line in buffer, see SDPContainer::Parse()
	/******************  ʵ�ʴ�ŵ���SDP����  ***************************************/
    StrPtrLen   fSDPBuffer;// sdp buffer which we really counter and is input stream
    UInt16      fReqLines; //the request sdp line,������'vosi'��?

    /* char array consists of fHeaderType in each sdp line with the default max size 256 */
	/* ��Щ��ʶ�ַ���������ASCII����е�λ�ó��� */
    char        fFieldStr[kLineTypeArraySize]; // �������飬���ü���

};

/** used in QTSSFileModule::DoDescribe(), two functions SDPLineSorter() and GetSortedSDPCopy() see SDPUtils.cpp */
class SDPLineSorter {

public:
	SDPLineSorter(): fSessionLineCount(0),fSDPSessionHeaders(NULL,0), fSDPMediaHeaders(NULL,0) {};//���캯��
	/*************************����������*****************************************************************/
	SDPLineSorter(SDPContainer *rawSDPContainerPtr, Float32 adjustMediaBandwidthPercent = 1.0);//��һ�����캯��
	
	/* obtain Session and Media headers */
	StrPtrLen* GetSessionHeaders() { return &fSessionHeaders; }//fSessionHeaders�������
	StrPtrLen* GetMediaHeaders() { return &fMediaHeaders; } //fMediaHeaders�������

	/* get sorted sdp line copy */
	char* GetSortedSDPCopy();
	
	/*****************************************************************************************************/
	//
	// member variables
	//
	/* sdp line buffer, SPL����StrPtrLen */
	StrPtrLen fullSDPBuffSPL;

	/* total number of sdp session-level line */
	/* �ҵ�SDP�е��к�(��0��ʼ) */
	SInt32 fSessionLineCount;

	/* sdp container with a sdp session  */
	/* ע���������class SDPContainer��ϵ����  */
	SDPContainer fSessionSDPContainer;

	/* sdp session and media header  */
	/* ��sSessionOrderedLines[]= "vosiuepcbtrzka"Ϊ�̶�˳��,������ͬ����ĸ��ͷ���з���һ��,�γ�fSDPSessionHeaders */
	/* �μ�SDPLineSorter(SDPContainer *,Float32)  */
	ResizeableStringFormatter fSDPSessionHeaders;
	/* ��m��ͷ�����Ժ��ҵ����ܵ�b��ͷ����,������MediaBandwidth,����ʼ����Ա����fMediaHeaders */
	ResizeableStringFormatter fSDPMediaHeaders;

	/* session and media header  */

	/******************************************************************************/
	//���������ǳ���Ҫ.�������һ��SDP�ļ�.
	/* ��fSDPSessionHeaders���Ľ������ΪfSessionHeaders */
	/* �μ�SDPLineSorter(SDPContainer *,Float32)  */
	StrPtrLen fSessionHeaders; //fSessionHeaders����
	/*****��fSDPMediaHeaders��һ�г�ʼ����һ��Ա����fMediaHeaders,�ҳ���m��ͷ����,�Ժ���п��ܰ���b��ͷ����*****/
	StrPtrLen fMediaHeaders;//fMediaHeaders���壬���ü�����

    /* array of fixed-orded sdp session line */
	/* ���SDPContainer::Parse() */
	static char sSessionOrderedLines[];// = "vosiuepcbtrzka"; // chars are order dependent: declared by rfc 2327
	static char sessionSingleLines[];//  = "vosiuepcbzk";    // return only 1 of each of these session field types
	
	/* the following  both see SDPUtils.cpp */
	/* spd line stop char */
	static StrPtrLen sEOL;
	/* max bandwidth */
    static StrPtrLen sMaxBandwidthTag;
};


#endif

