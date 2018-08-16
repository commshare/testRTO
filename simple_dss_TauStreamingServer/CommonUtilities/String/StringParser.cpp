/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 StringParser.cpp
Description: A couple of handy utilities for parsing a stream.
Comment:     copy from Darwin Streaming Server 5.5.5 
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 



#include "StringParser.h"

/* some mask array constants def */

/* ֻҪ����Ӣ����ĸ��ͣ������Ӣ����ĸ����Щ��������Ϊ1 */
UInt8 StringParser::sNonWordMask[] =
{
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //0-9 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //10-19 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //20-29
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //30-39 
    1, 1, 1, 1, 1, 0, 1, 1, 1, 1, //40-49 - is a word
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //50-59
    1, 1, 1, 1, 1, 0, 0, 0, 0, 0, //60-69 //stop on every character except a letter
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
    0, 1, 1, 1, 1, 0, 1, 0, 0, 0, //90-99 _ is a word
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //100-109
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
    0, 0, 0, 1, 1, 1, 1, 1, 1, 1, //120-129
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //130-139
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //140-149
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //150-159
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //160-169
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //170-179
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //180-189
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //190-199
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //200-209
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //210-219
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //220-229
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //230-239
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //240-249
    1, 1, 1, 1, 1, 1             //250-255
};

UInt8 StringParser::sWordMask[] =
{
    // Inverse of the above
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //0-9 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //10-19 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //20-29
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //30-39 
    0, 0, 0, 0, 0, 1, 0, 0, 0, 0, //40-49 - is a word
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //50-59
    0, 0, 0, 0, 0, 1, 1, 1, 1, 1, //60-69 //stop on every character except a letter
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //70-79
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //80-89
    1, 0, 0, 0, 0, 1, 0, 1, 1, 1, //90-99 _ is a word
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //100-109
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //110-119
    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, //120-129
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //130-139
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //140-149
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //150-159
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //160-169
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //170-179
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //180-189
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //190-199
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //200-209
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //210-219
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //220-229
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //230-239
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //240-249
    0, 0, 0, 0, 0, 0             //250-255
};

UInt8 StringParser::sDigitMask[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //0-9
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //10-19 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //20-29
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //30-39
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, //40-49 //stop on every character except a number
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, //50-59
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //60-69 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //90-99
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //100-109
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //120-129
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //130-139
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //140-149
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //150-159
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //160-169
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //170-179
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //180-189
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //190-199
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //200-209
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //210-219
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //220-229
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //230-239
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //240-249
    0, 0, 0, 0, 0, 0             //250-255
};

UInt8 StringParser::sEOLMask[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //0-9   
    1, 0, 0, 1, 0, 0, 0, 0, 0, 0, //10-19    //'\r' & '\n' are stop conditions
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //20-29
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //30-39 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //40-49
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //50-59
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //60-69  
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //90-99
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //100-109
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //120-129
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //130-139
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //140-149
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //150-159
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //160-169
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //170-179
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //180-189
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //190-199
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //200-209
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //210-219
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //220-229
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //230-239
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //240-249
    0, 0, 0, 0, 0, 0             //250-255
};

UInt8 StringParser::sWhitespaceMask[] =
{
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, //0-9      // stop on '\t'
    0, 0, 0, 0, 1, 1, 1, 1, 1, 1, //10-19    // '\r', \v', '\f' & '\n'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //20-29
    1, 1, 0, 1, 1, 1, 1, 1, 1, 1, //30-39   //  ' '
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //40-49
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //50-59
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //60-69
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //70-79
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //80-89
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //90-99
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //100-109
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //110-119
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //120-129
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //130-139
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //140-149
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //150-159
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //160-169
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //170-179
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //180-189
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //190-199
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //200-209
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //210-219
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //220-229
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //230-239
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //240-249
    1, 1, 1, 1, 1, 1             //250-255
};

/* ���������������ƺ���Щ�෴,����Ƚ�RTSPRequest::sURLStopConditions[] */
UInt8 StringParser::sEOLWhitespaceMask[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, //0-9     // \t is a stop
    1, 1, 1, 1, 0, 0, 0, 0, 0, 0, //10-19    //'\r' & '\n' are stop conditions
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //20-29
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, //30-39   ' '  is a stop
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //40-49
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //50-59
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //60-69  
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //90-99
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //100-109
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //120-129
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //130-139
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //140-149
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //150-159
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //160-169
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //170-179
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //180-189
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //190-199
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //200-209
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //210-219
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //220-229
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //230-239
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //240-249
    0, 0, 0, 0, 0, 0             //250-255
};


/* �������ַ�����Ϊָ����ֹ��ǰ���ַ��������������κ�ֵ */
/* ע�����outString���Ƿǿ�,�������������outString�������ø������¸�ֵ */
/* move fStartGet to before inStop and nothing return */
/* �ǳ����ĵĺ���,һ��Ҫ���洧Ħ */
/* FUNDAMENTAL FUNCTION */
/* note outString's value is not important ! */
void StringParser::ConsumeUntil(StrPtrLen* outString, char inStop)
{
	/* if fStartGet or fEndGet pointer is null, quit immediatly */
	/* if outString is not null, then make it null */
	/* ע��: fStartGet or fEndGet��outString���κι�ϵ!! */
    if (this->ParserIsEmpty(outString))
        return;

	/* store original fStartGet value for later use, because its value will change below */
	/** ʱ��ע��fStartGet��λ��,���ڳ��򲢷����ַ�����ͷ ***************/
    char *originalStartGet = fStartGet;

	/* fStartGet always step forward by 1 */
	/* ע�������Ƿ����������ƶ�,ֱ��ͣ��inStop֮ǰ */
    while ((fStartGet < fEndGet) && (*fStartGet != inStop))
		/* ע����ÿ���ƶ�������Ҫ��ͣ�ؼ����βָ��ָ���Ƿ�Ϊ�գ��Լ��Ƿ��� */
        AdvanceMark();
    
	/* adjust outString if it is not null */
	/* ���outString�ǿյ�,��û��Ҫ������ */
    if (outString != NULL)
    {
		/* use original fStartGet value */
        outString->Ptr = originalStartGet;
		/* note Len just smaller than before by 1 and fStartGet stop just before inStop */
        outString->Len = fStartGet - originalStartGet;
    }
}

/* FUNDAMENTAL FUNCTION */
/* ע��ڶ�������������Ļ�����������ָ�����������ʹ��ָ����Ϊ��ʹ������,���ս����ȫ��ͬ������ʹ�ø���� */
/* ʵ������������õø���,����Ҫ!! */
void StringParser::ConsumeUntil(StrPtrLen* outString, UInt8 *inMask)
{
	/* ȷ��fStartGet��fEndGet������Ϊ�� */
    if (this->ParserIsEmpty(outString))
        return;
        
    char *originalStartGet = fStartGet;

	/* ��fStartGetָ����ַ�����ģ�ڸ�(��ֵΪ0)ʱfStartGetָ��ǰ��һλ */
    while ((fStartGet < fEndGet) && (!inMask[*fStartGet]))
        AdvanceMark();

    if (outString != NULL)
    {
        outString->Ptr = originalStartGet;
        outString->Len = fStartGet - originalStartGet;
    }
}

/* fStartGet move forward or backwards a certain length  */
/* ע��spl��fStartGet�ƶ�ǰ������ֵ */
void StringParser::ConsumeLength(StrPtrLen* spl, SInt32 inLength)
{
	/* assure fStartGet and fEndGet not null */
    if (this->ParserIsEmpty(spl))
        return;

    //sanity check(���Ǽ��) to make sure we aren't being told to run off the end of the
    //buffer
	/* adjust the length of string */
    if ((fEndGet - fStartGet) < inLength)
        inLength = fEndGet - fStartGet;
    
	/* adjust the two member variables of spl */
    if (spl != NULL)
    {
        spl->Ptr = fStartGet;
        spl->Len = inLength;
    }
    if (inLength > 0)
    {
        for (short i=0; i<inLength; i++)
			/* step forward by 1 each time */
			/* �˶��лῼ�ǵ����� */
            AdvanceMark();
    }
    else
		/* otherwise move backwards, note inLength may be negative value */
        fStartGet += inLength;  // ***may mess up line number if we back up too much
}

/* fetch all digit number in outString in turn to give a decimal integer */
/* ע��˴�fStartGetֻ����ȡ��������һ�������ʱ����,�����ַ�ɢʱ��ֻ��һ��һ��ȡ,����Խ�м�Ŀո�� */
UInt32 StringParser::ConsumeInteger(StrPtrLen* outString)
{
	/* assure fStartGet and fEndGet not null */
	/* �м�fStartGet and fEndGet��outStringû�й�ϵ,�����ǲ���Ҫ��ʱ,outStringҲ����ΪNULL */
    if (this->ParserIsEmpty(outString))
        return 0;

    UInt32 theValue = 0;
	/* save the original fStartGet value for later use */
	/* ע����StringParse����Ҫʱ��ע��fStartGet��һ��һ��!! */
    char *originalStartGet = fStartGet;
    
	/* fetch all digit number in outString in turn to give a decimal integer */
    while ((fStartGet < fEndGet) && (*fStartGet >= '0') && (*fStartGet <= '9'))
    {
        theValue = (theValue * 10) + (*fStartGet - '0');
		/* fStartGet step forward 1 each time */
        AdvanceMark();
    }

	/* truncate outString by the last fStartGet position  */
	/* update the outString if necessary,but possibility is minor */
    if (outString != NULL)
    {
        outString->Ptr = originalStartGet;
        outString->Len = fStartGet - originalStartGet;
    }
    return theValue;
}

/* fetch all digit number in outString in turn to give a decimal float number */
Float32 StringParser::ConsumeFloat()
{
	/* assure fStartGet and fEndGet not null */
    if (this->ParserIsEmpty(NULL))
        return 0.0;

	/* fetch the digit number before decimal point */
    Float32 theFloat = 0;
    while ((fStartGet < fEndGet) && (*fStartGet >= '0') && (*fStartGet <= '9'))
    {
        theFloat = (theFloat * 10) + (*fStartGet - '0');
        AdvanceMark();
    }

	/* fetch the decimal point */
    if ((fStartGet < fEndGet) && (*fStartGet == '.'))
        AdvanceMark();
    Float32 multiplier = (Float32) .1;

	/* furthermore fetch the decimal digit figure after the decimal point */
    while ((fStartGet < fEndGet) && (*fStartGet >= '0') && (*fStartGet <= '9'))
    {
        theFloat += (multiplier * (*fStartGet - '0'));
        multiplier *= (Float32).1;

        AdvanceMark();
    }
    return theFloat;
}


//These functions consume(����,����) the given token/word if it is in the stream.
//If not, they return false.In all other situations, true is returned.
/* �����ڹ��ĵ�������:fStartGet�Ƿ��ƶ�?�����������ƶ� */
/* �ж�ָ��fStartGet��ǰ�Ƿ�ָ�����stopChar,����,����AdvanceMark(),fStartGetǰ��һ��,�������Ƿ����к�;���򷵻�false */
Bool16  StringParser::Expect(char stopChar)
{
	/* assure fStartGet and fEndGet not null */
	/* ȷ��fStartGet��fEndGet�ǿ� */
    if (this->ParserIsEmpty(NULL))
        return false;

    if (fStartGet >= fEndGet)
        return false;
    if(*fStartGet != stopChar)
        return false;
    else
    {
        AdvanceMark();
        return true;
    }
}

/** two helpful functions for dealing with http/rtsp string */

/* detect the eol */
/* �ú�����������̽��fStartGet��ǰָ����Ƿ���eol������Ӧ�ƶ�,�Ƿ��кŲż�1���жϷ�������: */
/* ��fStartGet��ǰָ�����'\n'��'\r'(���治�ý���'\n')ʱ�кŲż�1,��fStartGet��ǰָ�����'\n'��'\r'ʱ����true */
Bool16 StringParser::ExpectEOL()
{
	/* assure fStartGet and fEndGet not null */
    if (this->ParserIsEmpty(NULL))
        return false;

    //This function processes all legal forms of HTTP / RTSP eols.
    //They are: \r (alone), \n (alone), \r\n
    Bool16 retVal = false;
    if ((fStartGet < fEndGet) && ((*fStartGet == '\r') || (*fStartGet == '\n')))
    {
        retVal = true;
        AdvanceMark();
        //check for a \r\n, which is the most common EOL sequence.
        if ((fStartGet < fEndGet) && ((*(fStartGet - 1) == '\r') && (*fStartGet == '\n')))
            AdvanceMark();
    }
    return retVal;
}

//This function processes all legal forms of HTTP / RTSP eols.
//They are: \r (alone), \n (alone), \r\n
/* return the string from the beginning to '\r' or/and '\n' terminated */
/* ��������������Ǵ���fStartGet��eol��ʼ�ƶ�һλ�����е�����,���������ַ�����outString */
void StringParser::ConsumeEOL(StrPtrLen* outString)
{
	/* assure fStartGet and fEndGet not null */
    if (this->ParserIsEmpty(outString))
        return;

	/* save the original fStartGet value for later use */
	char *originalStartGet = fStartGet;
	
	/* only select one of '\r' and '\n' */
	if ((fStartGet < fEndGet) && ((*fStartGet == '\r') || (*fStartGet == '\n')))
	{
		/* if *fStartGet is '\n', then remember line-number increase 1 */
		/* ����*fStartGet��'\r'���治��'\n',�кŲż�1,���ֻ����'\r'������ȷ���Ƿ��кŲż�1 */
		AdvanceMark();
		//further check for a \r\n, which is the most common EOL sequence.
		if ((fStartGet < fEndGet) && ((*(fStartGet - 1) == '\r') && (*fStartGet == '\n')))
			/* remember line-number increase 1  */
			AdvanceMark();
	}

	if (outString != NULL)
	{
		outString->Ptr = originalStartGet;
		/* note Len just decrease by the delimiter '\r' or '\n' */
		outString->Len = fStartGet - originalStartGet;
	}
}

/* remove the single/double quotes in outString */
void StringParser::UnQuote(StrPtrLen* outString)
{
    // If a string is contained within double or single quotes 
    // then UnQuote() will remove them. - [sfu]
    
    // sanity check(���Ǽ��)
    if (outString->Ptr == NULL || outString->Len < 2)
        return;
        
    // remove begining quote if it's there.
    if (outString->Ptr[0] == '"' || outString->Ptr[0] == '\'')
    {
        outString->Ptr++; outString->Len--;
    }
    // remove ending quote if it's there.
    if ( outString->Ptr[outString->Len-1] == '"' || 
         outString->Ptr[outString->Len-1] == '\'' )
    {
        outString->Len--;
    }
}

/* fStartGet  always steps forward 1  */
/* NOTE: compare AdvanceMark() with fStartGet++ */
/* �ǳ���Ҫ�ĺ���,����ʮ��Ƶ���� */
/* �ú���������,����������ǰ�ƶ�һλ,ͬʱ�����ƶ��������Ƿ��� */
void StringParser::AdvanceMark()
{
	/* if this is null string, then quit */
	/* check tha fStatGet or fEndGet if is null, if is then return true, else return false */
	/* ��������fStatGet��fEndGet�Ƿ�Ϊ��,�����NULL�޹�,NULL��������Ǹ�������,����Ҫ��fStatGet�������ַ�������� */
     if (this->ParserIsEmpty(NULL))
        return;

	 /* �����������˵,��������'\r'(���治�ܽ���'\n')����'\n'�ͻ��� */
   if ((*fStartGet == '\n') || ((*fStartGet == '\r') && (fStartGet[1] != '\n')))
    {
        // we are progressing beyond a line boundary (don't count \r\n twice)
        fCurLineNumber++;
    }
   /* ע��������ǰ�ƶ�һλ */
    fStartGet++;
}

#if STRINGPARSERTESTING
Bool16 StringParser::Test()
{
    static char* string1 = "RTSP 200 OK\r\nContent-Type: MeowMix\r\n\t   \n3450";
    
    StrPtrLen theString(string1, strlen(string1));
    
    StringParser victim(&theString);
    
    StrPtrLen rtsp;
    SInt32 theInt = victim.ConsumeInteger();
    if (theInt != 0)
        return false;
    victim.ConsumeWord(&rtsp);
    if ((rtsp.len != 4) && (strncmp(rtsp.Ptr, "RTSP", 4) != 0))
        return false;
        
    victim.ConsumeWhiteSpace();
    theInt = victim.ConsumeInteger();
    if (theInt != 200)
        return false;
        
    return true;
}
#endif
