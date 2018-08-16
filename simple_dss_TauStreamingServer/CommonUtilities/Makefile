# Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
# modified by taoyunxing@dadimedia.com 
# last update 2010-08-20

NAME = CommonUtilitiesLib
C++ = $(CPLUS)
CC = $(CCOMP)
LINK = $(LINKER)
CCFLAGS += $(COMPILER_FLAGS) $(INCLUDE_FLAG) ../Build/PlatformHeader.h -g -Wall

# OPTIMIZATION
CCFLAGS += -O2

# EACH DIRECTORY WITH HEADERS MUST BE APPENDED IN THIS MANNER TO THE CCFLAGS

CCFLAGS += -I.
CCFLAGS += -IEncrypt
CCFLAGS += -IOSUtilities
CCFLAGS += -IOthers
CCFLAGS += -ISocket
CCFLAGS += -IString
CCFLAGS += -IStringSafeStdLib
CCFLAGS += -ITask
CCFLAGS += -I../Build
CCFLAGS += -I../APIModules/APIStubLib
CCFLAGS += -I../ServerCore/RTP
CCFLAGS += -I../ServerCore/SDP
CCFLAGS += -I../ServerCore/RTSP
CCFLAGS += -I../APIModules/APICommonCode

C++FLAGS = $(CCFLAGS)

CFILES = ./Encrypt/base64.c ./Encrypt/md5.c

CPPFILES = ./Others/atomic.cpp\
			./Others/DateTranslator.cpp\
			./Others/UserAgentParser.cpp \
			./Others/MyAssert.cpp \
			./OSUtilities/OS.cpp\
			./OSUtilities/OSCodeFragment.cpp \
			./OSUtilities/OSCond.cpp\
			./OSUtilities/OSFileSource.cpp \
			./OSUtilities/OSHeap.cpp\
			./OSUtilities/OSBufferPool.cpp \
			./OSUtilities/OSMutex.cpp \
			./OSUtilities/OSMutexRW.cpp \
			./OSUtilities/OSQueue.cpp\
			./OSUtilities/OSRef.cpp \
			./OSUtilities/OSThread.cpp\
			./String/ResizeableStringFormatter.cpp \
			./String/StringFormatter.cpp\
			./String/StringParser.cpp \
			./String/StringTranslator.cpp\
			./String/StrPtrLen.cpp \
			./Task/Task.cpp\
			./Task/IdleTask.cpp\
			./Task/TimeoutTask.cpp \
			./Socket/Socket.cpp \
			./Socket/SocketUtils.cpp\
			./Socket/TCPSocket.cpp\
			./Socket/TCPListenerSocket.cpp\
			./Socket/UDPDemuxer.cpp\
			./Socket/UDPSocket.cpp \
			./Socket/UDPSocketPool.cpp\
			./Socket/ev.cpp \
			./Socket/EventContext.cpp\
			./Encrypt/md5digest.cpp \
			../ServerCore/SDP/SDPUtils.cpp

all: libCommonUtilitiesLib.a

libCommonUtilitiesLib.a: $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)
	ar -r libCommonUtilitiesLib.a $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)
	ranlib libCommonUtilitiesLib.a

install: libCommonUtilitiesLib.a

clean:
	rm -f libCommonUtilitiesLib.a $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)

.SUFFIXES: .cpp .c .o

.cpp.o:
	$(C++) -c -o $*.o $(DEFINES) $(C++FLAGS) $*.cpp

.c.o:
	$(CC) -c -o $*.o $(DEFINES) $(CCFLAGS) $*.c

