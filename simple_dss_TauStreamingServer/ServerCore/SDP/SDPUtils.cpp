
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 SDPUtils.cpp
Description: define some static routines for dealing with SDPs.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 



#include "OS.h"
#include "OSMemory.h"
#include "SDPUtils.h"
#include "StrPtrLen.h"
#include "StringParser.h"
#include "ResizeableStringFormatter.h"



/* add the given line to the next line of the current one and return this line number */
SInt32 SDPContainer::AddHeaderLine (StrPtrLen *theLinePtr)
{   
	/* assure in-para no null */
    Assert(theLinePtr);
	/* record number of actual, used lines which is the location of the newly-added line */
	/* 设定当前行行号# */
    UInt32 thisLine = fNumUsedLines;
	/* assure the number of used line no bigger than total line numbers */
    Assert(fNumUsedLines < fNumSDPLines);
	/* set paras of the (thisLine+1)-th line */
	/* 取fSDPLineArray中的指定字符,用继承自StrPtrLen的Set()来设定入参 */
	/* 注意该行开头还有一个标志位(SDPLine的成员),它的设置见下面 */
    fSDPLineArray[thisLine].Set(theLinePtr->Ptr, theLinePtr->Len);
	/* 实际用到的行数加1 */
    fNumUsedLines++;

	/* if increased line reach the border of array, then enlarge the size of original array to double  */
	/* 此处特意处理边界情况 */
    if (fNumUsedLines == fNumSDPLines)
    {
		/* 从第一行和最末行可推断出fNumUsedLines就是实际使用的sdp行数 */
        SDPLine   *tempSDPLineArray = NEW SDPLine[fNumSDPLines * 2];
		/* copy original fSDPLineArray[fNumSDPLines] to newly build tempSDPLineArray then delete the original one  */
        for (int i = 0; i < fNumSDPLines; i++)
        {
            tempSDPLineArray[i].Set(fSDPLineArray[i].Ptr,fSDPLineArray[i].Len);
            tempSDPLineArray[i].fHeaderType = fSDPLineArray[i].fHeaderType;
        }
        delete [] fSDPLineArray;
		/* revise back the original array and name */
        fSDPLineArray = tempSDPLineArray;
        fNumSDPLines = (fNumUsedLines * 2);
    }
    
	/* set header type of thisLine line */
	/* 将入参第第一个字符替代该行的派生类的成员变量fHeaderType(它的初始值是'\0') */
	/* 现在统一入参第一个字符和该行派生类的成员变量fHeaderType */
    if (theLinePtr->Ptr)
        fSDPLineArray[thisLine].fHeaderType = theLinePtr->Ptr[0];
    
	/* return line number */
	/* 返回当前行行号 */
    return thisLine;
}

/* find out which line  the sdp line with the given id when fHeaderType locates and return the line index */
/* 从编号为start的行开始寻找指定id的行,找到后返回该行的行号 */
SInt32 SDPContainer::FindHeaderLineType(char id, SInt32 start)
{   
    SInt32 theIndex = -1;
    
	/* assure the start has a rational range  */
    if (start >= fNumUsedLines || start < 0)
        return -1;
        
    for (int i = start; i < fNumUsedLines; i++)
		/* 先找到指定行的头标记fHeaderType,再和入参id比较 */
		/* compare given id and fHeaderType */
    {   if (fSDPLineArray[i].fHeaderType == id)
        {   theIndex = i;
	        /* current line has the given id */
            fCurrentLine = theIndex;
            break;
        }
    }
    
    return theIndex;
}

/* return pointer to fHeaderType of the next line of current sdp line */
SDPLine* SDPContainer::GetNextLine()
{
    if (fCurrentLine < fNumUsedLines)
    {   fCurrentLine ++;
        return &fSDPLineArray[fCurrentLine];
    }
    
    return NULL;

}

/* return pointer to fHeaderType of the sdp line with the given line index */
SDPLine* SDPContainer::GetLine(SInt32 lineIndex)
{
    /* the lineIndex has range from 0 to fNumUsedLines-1 */
    if (lineIndex > -1 && lineIndex < fNumUsedLines)
    {   return &fSDPLineArray[lineIndex];
    }

    return NULL;
}

/* set the current line to the line with given index */
/* 设定当前行的行号 */
void SDPContainer::SetLine(SInt32 index)
{
	/* the lineIndex has range from 0 to fNumUsedLines-1 */
    if (index > -1 && index < fNumUsedLines)
    {   fCurrentLine = index;
    }
    else
        Assert(0);
        
}

/* FUNDAMENTAL FUNCTION and always invoke the functions in StringParser.h  */
/* parse sdp line in buffer and add it to the next line of the current one */
/* and mark the valid status of this line */
void SDPContainer::Parse()
{
	/* string array consist of the line type char of each line */
	char*	    validChars = "vosiuepcbtrzkam";
	/* separater between name and concrete value */
	char        nameValueSeparator = '=';
	
	/* all kinds of  validity check */
	// check line if begin with one of the valid characters followed by an "="
	// check if line has whitespace after the "="
	// check if fNumUsedLines==0?...
	Bool16      valid = true;

	/* initialize and def object of StringParser */
	/* IMPORTANT CLASS TO USE LATER */

	/* sdpParser用在这里，是个非常地道的词，这里一起用到了StrPtrLen和StringParser这两个底层基类 */
	/* 下面新建一个StringParser对象来解析fSDPBuffer中的数据 */
	StringParser	sdpParser(&fSDPBuffer);//see StringParser.h,here fSDPBuffer is StrPtrLen存放的是SDP数据

	/* use to describe a sdp line to parse */
	/* 下面的三个对象在解析一个普通的sdp行时要用到 */
	/* 代表sdp中的一个普通行 */
	StrPtrLen		line; /* 仅分析每行第一个字符,检查是否空行和fReqLines,合法性检查完后可添加sdp行到当前行的下一行 */
	StrPtrLen 		fieldName;/* 专门代表域名,解析是否以合法字符("vosiuepcbtrzkam"之一)后跟'='开头 */
	StrPtrLen		space;/* 专门代表空格,解析'='以后的sdp行,是否'='后有空格 */
	
	/* parse the whole line,fieldName and whitespace of sdp line */
	/* 得到stream或fSDPbuffer中的首尾指针间的数据 */
	/* 这个循环逐行解析sdp数据的合法性,将合法的数据加到当前行的下一步,遇到不合法的数据就中断循环 */
	while ( sdpParser.GetDataRemaining() != 0 )// see StringParser.h
	{
		/* fStartGet continue to move until eol, and this part string as a substitute of line */
		/* then fStartGet continue to walk over eol */
		/* 这一行的作用是使fSDPBuffer中的sdp数据分割成一行一行,现在line代表sdp数据的一行,末尾没有eol  */
		sdpParser.GetThruEOL(&line);  // Read each line  

        /* initialize and def  another object of StringParser */
		/* sdp数据中的普通一行,下面着重分析它 */
        StringParser lineParser(&line);//此处特地解析这一行，注意这一行读进来后值已变化,末尾没有eol

		/* why? analysis the new sdp line and apply the fStartGet and fEndGet pointer of this new line not former pointers */
		/* 吸收掉空格,注意比较空格和'\0'的区别,前者范围更广,见StringParser.cpp */
		/* 遇到空格whitespace字符fStartGet就前进直到遇到non-whitespace就停下 */
		/* 现在line开头没有了空格 */
        lineParser.ConsumeWhitespace();//skip over leading whitespace

		/***********************************************************/
		//上面的两步GetThruEOL和ConsumeWhitespace实现了对一个SDP行掐头去尾的作用

		/* judge the valid length */
		/* 如果吸收掉空格后是空行,就跳过,从while循环处从新开始 */
        if (lineParser.GetDataRemaining() == 0) // must be an empty line
            continue;

		/* return the current char pointed to by the fStartGet */
		/* 取出line中的第一个字符(标识位,fStartGet当前指向的) */
		/* 这个函数返回fStartGet当前指向的字符或者'\0',注意fStartGet位置不动 */
        char firstChar = lineParser.PeekFast();
		/* means a blank line */
		/* 还没有被SDPContainer::AddHeaderLine()更新 */
        if (firstChar == '\0')
            continue; //skip over blank lines
     
		/* give the first char in this line to firstChar-th component in fieldname array whose size is 256  */
		/* store the firstChar into the corresponding position in ASCII form */
		/* 将line的标识符firstChar按它在ASCII表中的先后位置摆放在字符数组fFieldStr中,这个数组大小是256 */
		/* 显然这个字符数组是有很多空格间隙的,因为它的256个位置不可能填满 */
		/* 显然这时firstChar并非'\0' */
        fFieldStr[firstChar] = firstChar;
		/* 分情形讨论,当出现下面四个标识符时定义成员变量fReqLines的值 */
        switch (firstChar)
        {
            case 'v': fReqLines |= kV;
            break;
    
            case 's': fReqLines |= kS ;
            break;
    
            case 't': fReqLines |= kT ;
            break;
    
            case 'o': fReqLines |= kO ;
            break;
        
        }

		/* fStartGet go pass the first char and come to the second char '=' */
		/* 现在开始解析域名,fieldName已经变成sdp行line中的第一个字符(域名) */
		lineParser.ConsumeUntil(&fieldName, nameValueSeparator);

     /* The strchr() function searches for the first occurrence of a string inside another string.
		This function returns the rest of the string (from the matching point), or FALSE, if the 
		string to search for is not found.This function is an alias of the strstr() function.
        This function is case-sensitive. For a case-insensitive search, use stristr().
     */
        /* field name char must in valid char and its length is 1 */
		/* validChars = "vosiuepcbtrzkam"  */
		if ((fieldName.Len != 1) || (::strchr(validChars, fieldName.Ptr[0]) == NULL))
		{
			valid = false; // line doesn't begin with one of the valid characters followed by an "="
			break;
		}
		
		/* the '=' not exist  */
		/* note that the fStartGet has come to the current location nameValueSeparator */
		/* 注意fStartGet正指向"=",它会移过"="并返回true,但是若它当前没有指向"="就返回false */
		if (!lineParser.Expect(nameValueSeparator))
		{
			valid = false; // line doesn't have the "=" after the first char
			break;
		}
		
		/* 时刻注意fStartGet的位置,它在这里很重要,关系到程序的理解 */
		/* 注意指针fStartGet现在已经移过了'='，即将继续移动 */
		/* 将给定字符串变为指定终止符前的字符串，但不返回任何值 */
        /* 现在解析上面定义的第三个对象 */
		/* judge whether there exist whitespace in this sdp line */
		/* space应该是空的,因为sWhitespaceMask已经被吸收了  */
		/* 注意sWhitespaceMask和ConsumeUntil的定义,fStartGet会经过sWhitespaceMask,直到遇到非sWhitespaceMask,就停下  */
		/* 比较上面的lineParser.ConsumeWhitespace() */
		lineParser.ConsumeUntil(&space, StringParser::sWhitespaceMask);
		
		/* note fStartGet always move forward and no step backward */
		/* 断定"="后有空格,就中断 */
		if (space.Len != 0)
		{
			valid = false; // line has whitespace after the "=" 
			break;
		}
		/* add the parsed line to the next line of the current one and return this next line number# */
		AddHeaderLine(&line);
	}
	
	if (fNumUsedLines == 0) // didn't add any lines
	{   valid = false;
	}
	/* update valid member variable */
	fValid = valid;
	
}

/* initialize member function */
void SDPContainer::Initialize()
{
    fCurrentLine = 0;
    fNumUsedLines = 0;
	/* 删去该数组,以便重建新的数组 */
    delete [] fSDPLineArray;
	/* each sdp line is C-char with a header type '\0' */
	/* 每个sdpLine是'\0',这个数组就是形如"\0\0\0...\0" */
    fSDPLineArray = NEW SDPLine[fNumSDPLines]; 
    fValid = false;
    fReqLines = 0;
	/* string array consists of each line field name */
	/* 初始化每个sdp行为256个0 */
    ::memset(fFieldStr, sizeof(fFieldStr), 0);
}

/* IMPORTANT FUNCTION */
/* input the given string, store in buffer, then parse its validity and add it to the next sdp line to current line */
Bool16 SDPContainer::SetSDPBuffer(char *sdpBuffer) 
{ 
    /* first initialize and clear buffer */
    Initialize();
	/* set member fSDPBuffer value for non-null input-parameter */
    if (sdpBuffer != NULL)
		/* use StrPtrLen.Set() */
    {   fSDPBuffer.Set(sdpBuffer); 
       /* parse the validity of the above fSDPBuffer, then  add it to the next line to the current one  */
	   /* 利用循环逐行解析sdp数据的合法性,将合法的数据加到当前行的下一步,遇到不合法的数据就中断循环 */
	   /* 最后还要设定fValid */
        Parse(); 
    }
    /*check validity of returned fValid value, def in SDPUtils.h */
    return IsSDPBufferValid();
}

Bool16 SDPContainer::SetSDPBuffer(StrPtrLen *sdpBufferPtr)
{ 
    Initialize();
    if (sdpBufferPtr != NULL)
    {   fSDPBuffer.Set(sdpBufferPtr->Ptr, sdpBufferPtr->Len); 
        Parse(); 
    }
    
    return IsSDPBufferValid();
}


void  SDPContainer::PrintLine(SInt32 lineIndex)
{
    StrPtrLen *printLinePtr = GetLine(lineIndex);
    if (printLinePtr)
    {   printLinePtr->PrintStr();
        qtss_printf("\n");
    }

}

void  SDPContainer::PrintAllLines()
{
    if (fNumUsedLines > 0)
    {   for (int i = 0; i < fNumUsedLines; i++)
            PrintLine(i);
    }
    else
        qtss_printf("SDPContainer::PrintAllLines no lines\n"); 
}


/* def and initialize objects */
char SDPLineSorter::sSessionOrderedLines[] = "vosiuepcbtrzka"; // chars are order dependent: declared by rfc 2327
char SDPLineSorter::sessionSingleLines[]  = "vosiuepcbzk";    // return only 1 of each of these session field types
StrPtrLen  SDPLineSorter::sEOL("\r\n");
StrPtrLen  SDPLineSorter::sMaxBandwidthTag("b=AS:"); //AS=Application Specific Maximum

/* the non-default constructor function */
/* 初始化其它几个成员,并对SDP数据进行重排 */
SDPLineSorter::SDPLineSorter(SDPContainer *rawSDPContainerPtr, Float32 adjustMediaBandwidthPercent) : fSessionLineCount(0),fSDPSessionHeaders(NULL,0), fSDPMediaHeaders(NULL,0)
/* 其它的几个成员初始化如下:fMediaHeaders, fSessionSDPContainer,fSDPSessionHeaders,fSessionHeaders*/
{

	/* make sure the pointer no null */
	Assert(rawSDPContainerPtr != NULL);
	if (NULL == rawSDPContainerPtr) 
		return;
	
	/* define two objects of class StrPtrLen  */
	/* 用buffer中的sdp数据初始化 */
	StrPtrLen theSDPData(rawSDPContainerPtr->fSDPBuffer.Ptr,rawSDPContainerPtr->fSDPBuffer.Len);
	
	/* 从第0行开始,找到以m开头的那一行,返回该行行号,再取以m开头的那一行 */
	StrPtrLen *theMediaStart = rawSDPContainerPtr->GetLine(rawSDPContainerPtr->FindHeaderLineType('m',0));
	/* 从m开头的行以后找到可能的b开头的行,调整其MediaBandwidth,并初始化成员变量fMediaHeaders */
 	if (theMediaStart && theMediaStart->Ptr && theSDPData.Ptr)
	{
		/* 用m开头的一行(或几行)初始化, 它是SDP数据的最后一(几)行 */
		UInt32  mediaLen = theSDPData.Len - (UInt32) (theMediaStart->Ptr - theSDPData.Ptr);
		char *mediaStartPtr= theMediaStart->Ptr;
		/********************** 初始化成员变量fMediaHeaders****************/
		fMediaHeaders.Set(mediaStartPtr,mediaLen);
		/* 定义并初始化对象sdpParser */
        StringParser sdpParser(&fMediaHeaders);
		/* 代表普通的一行 */
        StrPtrLen sdpLine;
        while (sdpParser.GetDataRemaining() > 0)
        {
            sdpParser.GetThruEOL(&sdpLine);
            
            if ( ( 'b' == sdpLine.Ptr[0]) && (1.0 != adjustMediaBandwidthPercent) )
            {   
				/* 重新建一个对象 */
                StringParser bLineParser(&sdpLine);
				/* 行进到数字面前 */
                bLineParser.ConsumeUntilDigit();
				/* 取出其中的数字乘入参adjustMediaBandwidthPercent(50%) */
                UInt32 bandwidth = (UInt32) (.5 + (adjustMediaBandwidthPercent * (Float32) bLineParser.ConsumeInteger() ) );
                if (bandwidth < 1) 
                    bandwidth = 1;
                
				/* 得到给定格式的字符串bandwidthStr,且最后一个字符为0 */
                char bandwidthStr[10];
                qtss_snprintf(bandwidthStr,sizeof(bandwidthStr) -1, "%lu", bandwidth);
                bandwidthStr[sizeof(bandwidthStr) -1] = 0;
                
				/* 输出形为"b=AS:xxxxxx0"的字符串 */
                fSDPMediaHeaders.Put(sMaxBandwidthTag);
                fSDPMediaHeaders.Put(bandwidthStr);
            }
            else
				/* 否则输出其它的一行,以\r\n结尾 */
                fSDPMediaHeaders.Put(sdpLine);

            fSDPMediaHeaders.Put(SDPLineSorter::sEOL);
        } 
		/************************** 用fSDPMediaHeaders的一行初始化另一成员变量fMediaHeaders ***********/
		/*****第二次初始化成员变量fMediaHeaders,找出以m开头的行,以后的行可能包含b开头的行*****/
        fMediaHeaders.Set(fSDPMediaHeaders.GetBufPtr(),fSDPMediaHeaders.GetBytesWritten());
    }

	/* 从编号为0的行开始寻找m开头的行,找到后返回该行的行号 */
	fSessionLineCount = rawSDPContainerPtr->FindHeaderLineType('m',0);
	/* 假如找到的行的编号为负值,就更正为实际用到的行数 */
	if (fSessionLineCount < 0) // didn't find it use the whole buffer
	{   fSessionLineCount = rawSDPContainerPtr->GetNumLines();
	}

	/* 得到指定索引号的行加到当前行的下一行,直到找到行号的这行为止 */
	/* 注意已找到行号的m这行最先输出的,下面的行依次是第0,1,2,..行等 */
	for (SInt16 sessionLineIndex = 0; sessionLineIndex < fSessionLineCount; sessionLineIndex++)
		fSessionSDPContainer.AddHeaderLine( (StrPtrLen *) rawSDPContainerPtr->GetLine(sessionLineIndex));

	//qtss_printf("\nSession raw Lines:\n"); fSessionSDPContainer.PrintAllLines();

	/* sSessionOrderedLines[]= "vosiuepcbtrzka";  */
	SInt16 numHeaderTypes = sizeof(SDPLineSorter::sSessionOrderedLines) -1;//13

	/* 下面的循环以sSessionOrderedLines[]= "vosiuepcbtrzka"为固定顺序,将以相同的字母开头的行放在一起,形成fSDPSessionHeaders */
	for (SInt16 fieldTypeIndex = 0; fieldTypeIndex < numHeaderTypes; fieldTypeIndex ++)
	{
		/* 从第0行开始找以指定字符顺序的行的行号 */
		SInt32 lineIndex = fSessionSDPContainer.FindHeaderLineType(SDPLineSorter::sSessionOrderedLines[fieldTypeIndex], 0);
		/* 取出该行 */
		StrPtrLen *theHeaderLinePtr = fSessionSDPContainer.GetLine(lineIndex);
		/* 迭代找出以指定字母开头的行并放入fSDPSessionHeaders中并以'\r\n'结尾,直到找尽这样的行 */
		while (theHeaderLinePtr != NULL)
		{
			fSDPSessionHeaders.Put(*theHeaderLinePtr);
			fSDPSessionHeaders.Put(SDPLineSorter::sEOL);

			/* 若以指定字母开头的这一行出现在只能出现一次的行sessionSingleLines[]= "vosiuepcbzk"中,就立即退出 */
			if (NULL != ::strchr(sessionSingleLines, theHeaderLinePtr->Ptr[0] ) ) // allow 1 of this type: use first found
				break; // move on to next line type

			/* 从找到的行号后再次寻找是否还要以指定字母开头的行,找到就输出它的行号(与上次的行号不同) */
			lineIndex = fSessionSDPContainer.FindHeaderLineType(SDPLineSorter::sSessionOrderedLines[fieldTypeIndex], lineIndex + 1);
			/* 依旧取出该行,参与上面的循环 */
			theHeaderLinePtr = fSessionSDPContainer.GetLine(lineIndex);
		}
	}
	/* 将fSDPSessionHeaders最后的结果设置为fSessionHeaders */
	fSessionHeaders.Set(fSDPSessionHeaders.GetBufPtr(),fSDPSessionHeaders.GetBytesWritten());

}

/* 得到SortedSDP的一个copy,从fSessionHeaders和fMediaHeaders部分依次复制得到,返回内存指针 */
char* SDPLineSorter::GetSortedSDPCopy()
{
	char* fullbuffCopy = NEW char[fSessionHeaders.Len + fMediaHeaders.Len + 2];
	SInt32 buffPos = 0;
	memcpy(&fullbuffCopy[buffPos], fSessionHeaders.Ptr,fSessionHeaders.Len);
	buffPos += fSessionHeaders.Len;
	memcpy(&fullbuffCopy[buffPos], fMediaHeaders.Ptr,fMediaHeaders.Len);
	buffPos += fMediaHeaders.Len;
	fullbuffCopy[buffPos] = 0;	
	
	return fullbuffCopy;
}


