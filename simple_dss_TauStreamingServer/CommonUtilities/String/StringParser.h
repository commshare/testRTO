
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 StringParser.h
Description: A couple of handy utilities for parsing a stream.
Comment:     copy from Darwin Streaming Server 5.5.5 
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 



#ifndef __STRINGPARSER_H__
#define __STRINGPARSER_H__

#include "StrPtrLen.h"
#include "MyAssert.h"

#define STRINGPARSERTESTING 0


class StringParser
{
    public:
        
        StringParser(StrPtrLen *inStream)
            :   fStartGet(inStream == NULL ? NULL : inStream->Ptr),
                fEndGet(inStream == NULL ? NULL : inStream->Ptr + inStream->Len),
                fCurLineNumber(1),
                fStream(inStream) {}
        ~StringParser() {}
        
        // Built-in masks for common stop conditions
		/* 定义见StringParser.cpp */
        static UInt8 sDigitMask[];      // stop when you hit a digit
        static UInt8 sWordMask[];       // stop when you hit a word
        static UInt8 sEOLMask[];        // stop when you hit an eol(即"\r\n"换行和回车)
        static UInt8 sEOLWhitespaceMask[]; // stop when you hit an EOL or whitespace
        static UInt8 sWhitespaceMask[]; // skip over whitespace(空白)

        //GetBuffer:
        //Returns a pointer to the string object
		/* see SourceInfo.h */
        StrPtrLen*      GetStream() { return fStream; }
        
        //Expect:
        //These functions consume the given token/word if it is in the stream.
        //If not, they return false.def see StringParse.cpp.
        //In all other situations, true is returned.
        //NOTE: if these functions return an error, the object goes into a state where
        //it cannot be guaranteed to function correctly.
        Bool16          Expect(char stopChar);
		/* 该函数的作用是探测fStartGet当前指向的是否是eol并作相应移动,是否行号才加1的判断方法如下: */
		/* 当fStartGet当前指向的是'\n'或'\r'(后面不得紧跟'\n')时行号才加1,当fStartGet当前指向的是'\n'或'\r'时返回true */
        Bool16          ExpectEOL();
        
        //Returns the next word
		/* why? used in RTSPRequest::ParseFirstLine() */
		/* 只要不是英文字母就停，不是英文字母的那些符号掩码为1 */
		/* 按照StringParser::sNonWordMask[]和ConsumeUntil定义,是英文字母就运动,不是就停下 */
        void            ConsumeWord(StrPtrLen* outString = NULL)
                            { ConsumeUntil(outString, sNonWordMask); }

        //Returns all the data before inStopChar, explained see StringParser.cpp
		/* 将给定字符串变为指定终止符前的字符串，但不返回任何值 */
		/* move fStartGet to before inStop and nothing return */
		/* FUNDAMENTAL FUNCTION */
        void            ConsumeUntil(StrPtrLen* outString, char inStopChar);

        //Returns whatever integer is currently in the stream
		/* 注意此处fStartGet只是在取出集中在一起的数字时管用,当数字分散时它只能一个一个取,在逾越中间的空格后 */
        UInt32          ConsumeInteger(StrPtrLen* outString = NULL);
        Float32         ConsumeFloat();

        //Keeps on going until non-whitespace
		/* 这个函数的正确理解要联系sWhitespaceMask[]和ConsumeUntil()定义,见StringParser.cpp */
		/* 遇到空格whitespace字符就前进直到遇到non-whitespace就停下 */
		/* used in SDPUtils::SDPContainer::Parse() */
		/* compare the ConsumeWhitespace() with ConsumeUntilWhitespace() */
		/* why?sWhitespaceMask def see StringParser.cpp */
        void            ConsumeWhitespace()
                            { ConsumeUntil(NULL, sWhitespaceMask); }
        
        //Assumes 'stop' is a 255-char array of booleans.(see .cpp def). Set this array
        //to a mask of what the stop characters are. true means stop character.
        //You may also pass in one of the many prepackaged masks defined above.
		/* 注意第二个参数与上面的基本函数仅是指针的区别，这里使用指针是为了使用数组,最终结果完全相同，但是使用更灵活 */
        void            ConsumeUntil(StrPtrLen* spl, UInt8 *stop);


        //+ rt 8.19.99
        //returns whatever is avaliable until non-whitespace
		/* 参见StringParser::sEOLWhitespaceMask[]定义,遇到 sEOLWhitespaceMask就停下 */
        void            ConsumeUntilWhitespace(StrPtrLen* spl = NULL)
                            { ConsumeUntil( spl, sEOLWhitespaceMask); }
        /* 遇到数字就停下 */
        void            ConsumeUntilDigit(StrPtrLen* spl = NULL)
                            { ConsumeUntil( spl, sDigitMask); }

		/* fStartGet向前移动指定长度，注意spl是fStartGet移动前调整的值 */
		void			ConsumeLength(StrPtrLen* spl, SInt32 numBytes);

		/* 这个函数的作用是处理fStartGet从eol开始移动一位并换行的问题,将经过的字符串给outString */
		void			ConsumeEOL(StrPtrLen* outString);

        //GetThru:
        //Works very similar to ConsumeUntil except that it moves past the stop token,
        //and if it can't find the stop token it returns false, see definition below
        inline Bool16       GetThru(StrPtrLen* spl, char stop);
        inline Bool16       GetThruEOL(StrPtrLen* spl);//指定停止符就无须第二个参数

		/* Fundamental function */
		/* 检查首尾两指针fStartGet和fEndGet指向是否为空？默认返回值是false.注意fStartGet并不移动 */
        inline Bool16       ParserIsEmpty(StrPtrLen* outString);//def see the end line of the code page

        //Returns the current character, doesn't move past it.
        inline char     PeekFast() { if (fStartGet) return *fStartGet; else return '\0'; }
		/* 取第i+1个分量的运算[] */
        char operator[](int i) { Assert((fStartGet+i) < fEndGet);return fStartGet[i]; }
        
        //Returns some info about the stream

		/* the length of parsed data */
        UInt32          GetDataParsedLen() 
            { Assert(fStartGet >= fStream->Ptr); return (UInt32)(fStartGet - fStream->Ptr); }
        /* the length of received data */
        UInt32          GetDataReceivedLen()  
            { Assert(fEndGet >= fStream->Ptr); return (UInt32)(fEndGet - fStream->Ptr); }
		/* the length of remaining data to parse */
		/*********************************************************************************************/
        UInt32          GetDataRemaining()         //this is important function used frequently
            { Assert(fEndGet >= fStartGet); return (UInt32)(fEndGet - fStartGet); }
		/* obtain the current position */
        char*           GetCurrentPosition() { return fStartGet; }
		/* obtain the number of current line */
        int         GetCurrentLineNumber() { return fCurLineNumber; }
        
        // A utility for extracting quotes from the start and end of a parsed
        // string. (Warning: Do not call this method if you allocated your own  
        // pointer for the Ptr field of the StrPtrLen class.) - [sfu]
        // 
        // Not sure why this utility is here and not in the StrPtrLen class - [jm]
		/* remove the single/double quotes in outString */
        static void UnQuote(StrPtrLen* outString);


#if STRINGPARSERTESTING
        static Bool16       Test();
#endif

    private:

		/* definition see StringParse.cpp. */
		/* NOTE: compare AdvanceMark() with fStartGet++ */
		/* 非常重要的函数,引用十分频繁！ */
		/* 该函数的作用,就是总是向前移动一位,同时决定移动过程中是否换行 */
        void        AdvanceMark();
        
        //built in masks for some common stop conditions
		/* def see StringParser.cpp */
        static UInt8 sNonWordMask[];

		/******** 注意fStartGet这个成员的移动非常重要,我们要始终关注它的一举一动! **********/
        char*       fStartGet;
        char*       fEndGet;
		/* point out which line to parse */
        int         fCurLineNumber;
		/* input stream in buffer, see SourceInfo.h */
        StrPtrLen*  fStream;
        
};

/* compare the GetThru and ConsumeUntil functions  */

Bool16 StringParser::GetThru(StrPtrLen* outString, char inStopChar)
{   
	/* 将outString变为inStopChar以前的字符串，但不返回任何值 */
    ConsumeUntil(outString, inStopChar);
	/* 头指针fStartGet继续移过inStopChar并返回true */
    return Expect(inStopChar);
}

Bool16 StringParser::GetThruEOL(StrPtrLen* outString)
{
    ConsumeUntil(outString, sEOLMask);
    return ExpectEOL();
}

/* VERY IMPORTANT FUNCTION  */
/* check tha fStatGet or fEndGet if is null, if is then return true, else return false */
/* 检查首尾两指针fStartGet和fEndGet指向是否为空？默认返回值是false.注意fStartGet并不移动 */
Bool16 StringParser::ParserIsEmpty(StrPtrLen* outString)
{
	/* note the values offStatGet or fEndGe have nothing with outString */
    if (NULL == fStartGet || NULL == fEndGet)
    {
		/* make sure that the outString is null */
        if (NULL != outString)
        {   outString->Ptr = NULL;
            outString->Len = 0;
        }
        
        return true;
    }
    
    Assert(fStartGet <= fEndGet);
    
    return false; // parser ok to parse
}


#endif // __STRINGPARSER_H__
