
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
		/* �����StringParser.cpp */
        static UInt8 sDigitMask[];      // stop when you hit a digit
        static UInt8 sWordMask[];       // stop when you hit a word
        static UInt8 sEOLMask[];        // stop when you hit an eol(��"\r\n"���кͻس�)
        static UInt8 sEOLWhitespaceMask[]; // stop when you hit an EOL or whitespace
        static UInt8 sWhitespaceMask[]; // skip over whitespace(�հ�)

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
		/* �ú�����������̽��fStartGet��ǰָ����Ƿ���eol������Ӧ�ƶ�,�Ƿ��кŲż�1���жϷ�������: */
		/* ��fStartGet��ǰָ�����'\n'��'\r'(���治�ý���'\n')ʱ�кŲż�1,��fStartGet��ǰָ�����'\n'��'\r'ʱ����true */
        Bool16          ExpectEOL();
        
        //Returns the next word
		/* why? used in RTSPRequest::ParseFirstLine() */
		/* ֻҪ����Ӣ����ĸ��ͣ������Ӣ����ĸ����Щ��������Ϊ1 */
		/* ����StringParser::sNonWordMask[]��ConsumeUntil����,��Ӣ����ĸ���˶�,���Ǿ�ͣ�� */
        void            ConsumeWord(StrPtrLen* outString = NULL)
                            { ConsumeUntil(outString, sNonWordMask); }

        //Returns all the data before inStopChar, explained see StringParser.cpp
		/* �������ַ�����Ϊָ����ֹ��ǰ���ַ��������������κ�ֵ */
		/* move fStartGet to before inStop and nothing return */
		/* FUNDAMENTAL FUNCTION */
        void            ConsumeUntil(StrPtrLen* outString, char inStopChar);

        //Returns whatever integer is currently in the stream
		/* ע��˴�fStartGetֻ����ȡ��������һ�������ʱ����,�����ַ�ɢʱ��ֻ��һ��һ��ȡ,����Խ�м�Ŀո�� */
        UInt32          ConsumeInteger(StrPtrLen* outString = NULL);
        Float32         ConsumeFloat();

        //Keeps on going until non-whitespace
		/* �����������ȷ���Ҫ��ϵsWhitespaceMask[]��ConsumeUntil()����,��StringParser.cpp */
		/* �����ո�whitespace�ַ���ǰ��ֱ������non-whitespace��ͣ�� */
		/* used in SDPUtils::SDPContainer::Parse() */
		/* compare the ConsumeWhitespace() with ConsumeUntilWhitespace() */
		/* why?sWhitespaceMask def see StringParser.cpp */
        void            ConsumeWhitespace()
                            { ConsumeUntil(NULL, sWhitespaceMask); }
        
        //Assumes 'stop' is a 255-char array of booleans.(see .cpp def). Set this array
        //to a mask of what the stop characters are. true means stop character.
        //You may also pass in one of the many prepackaged masks defined above.
		/* ע��ڶ�������������Ļ�����������ָ�����������ʹ��ָ����Ϊ��ʹ������,���ս����ȫ��ͬ������ʹ�ø���� */
        void            ConsumeUntil(StrPtrLen* spl, UInt8 *stop);


        //+ rt 8.19.99
        //returns whatever is avaliable until non-whitespace
		/* �μ�StringParser::sEOLWhitespaceMask[]����,���� sEOLWhitespaceMask��ͣ�� */
        void            ConsumeUntilWhitespace(StrPtrLen* spl = NULL)
                            { ConsumeUntil( spl, sEOLWhitespaceMask); }
        /* �������־�ͣ�� */
        void            ConsumeUntilDigit(StrPtrLen* spl = NULL)
                            { ConsumeUntil( spl, sDigitMask); }

		/* fStartGet��ǰ�ƶ�ָ�����ȣ�ע��spl��fStartGet�ƶ�ǰ������ֵ */
		void			ConsumeLength(StrPtrLen* spl, SInt32 numBytes);

		/* ��������������Ǵ���fStartGet��eol��ʼ�ƶ�һλ�����е�����,���������ַ�����outString */
		void			ConsumeEOL(StrPtrLen* outString);

        //GetThru:
        //Works very similar to ConsumeUntil except that it moves past the stop token,
        //and if it can't find the stop token it returns false, see definition below
        inline Bool16       GetThru(StrPtrLen* spl, char stop);
        inline Bool16       GetThruEOL(StrPtrLen* spl);//ָ��ֹͣ��������ڶ�������

		/* Fundamental function */
		/* �����β��ָ��fStartGet��fEndGetָ���Ƿ�Ϊ�գ�Ĭ�Ϸ���ֵ��false.ע��fStartGet�����ƶ� */
        inline Bool16       ParserIsEmpty(StrPtrLen* outString);//def see the end line of the code page

        //Returns the current character, doesn't move past it.
        inline char     PeekFast() { if (fStartGet) return *fStartGet; else return '\0'; }
		/* ȡ��i+1������������[] */
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
		/* �ǳ���Ҫ�ĺ���,����ʮ��Ƶ���� */
		/* �ú���������,����������ǰ�ƶ�һλ,ͬʱ�����ƶ��������Ƿ��� */
        void        AdvanceMark();
        
        //built in masks for some common stop conditions
		/* def see StringParser.cpp */
        static UInt8 sNonWordMask[];

		/******** ע��fStartGet�����Ա���ƶ��ǳ���Ҫ,����Ҫʼ�չ�ע����һ��һ��! **********/
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
	/* ��outString��ΪinStopChar��ǰ���ַ��������������κ�ֵ */
    ConsumeUntil(outString, inStopChar);
	/* ͷָ��fStartGet�����ƹ�inStopChar������true */
    return Expect(inStopChar);
}

Bool16 StringParser::GetThruEOL(StrPtrLen* outString)
{
    ConsumeUntil(outString, sEOLMask);
    return ExpectEOL();
}

/* VERY IMPORTANT FUNCTION  */
/* check tha fStatGet or fEndGet if is null, if is then return true, else return false */
/* �����β��ָ��fStartGet��fEndGetָ���Ƿ�Ϊ�գ�Ĭ�Ϸ���ֵ��false.ע��fStartGet�����ƶ� */
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
