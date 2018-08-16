# Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
# modified by taoyunxing@dadimedia.com 
# last update 2011-07-02 

NAME = DarwinStreamingServer
C++ = $(CPLUS)
CC = $(CCOMP)
LINK = $(LINKER)
CCFLAGS += $(COMPILER_FLAGS)  -DDSS_USE_API_CALLBACKS -g -Wall -Wno-format-y2k $(INCLUDE_FLAG) ../Build/PlatformHeader.h
LIBS = $(CORE_LINK_LIBS) -lCommonUtilitiesLib -lQTFileLib

# OPTIMIZATION
CCFLAGS += -O3

# EACH DIRECTORY WITH HEADERS MUST BE APPENDED IN THIS MANNER TO THE CCFLAGS

CCFLAGS += -I.
CCFLAGS += -I../MP4Packetizer
CCFLAGS += -I../APIModules/APICommonCode
CCFLAGS += -I../APIModules/APIStubLib
CCFLAGS += -I../APIModules/QTSSErrorLogModule
CCFLAGS += -I../APIModules/OSMemory_Modules
CCFLAGS += -I../APIModules/QTSSAccessLogModule
CCFLAGS += -I../APIModules/QTSSFileModule
CCFLAGS += -I../APIModules/QTSSFlowControlModule
CCFLAGS += -I../APIModules/QTSSPOSIXFileSysModule
CCFLAGS += -I../CommonUtilities/SafeStdLib
CCFLAGS += -I../CommonUtilities/Encrypt
CCFLAGS += -I../CommonUtilities/OSUtilities
CCFLAGS += -I../CommonUtilities/Others
CCFLAGS += -I../CommonUtilities/Socket
CCFLAGS += -I../CommonUtilities/String
CCFLAGS += -I../CommonUtilities/Task
CCFLAGS += -I../Build
CCFLAGS += -ISDP
CCFLAGS += -IHTTP
CCFLAGS += -IRTSP
CCFLAGS += -IRTP
CCFLAGS += -IRTCP
CCFLAGS += -IPrefs


# EACH DIRECTORY WITH A STATIC LIBRARY MUST BE APPENDED IN THIS MANNER TO THE LINKOPTS

LINKOPTS = -L../MP4Packetizer/
LINKOPTS += -L../CommonUtilities/

C++FLAGS = $(CCFLAGS)

CFILES = ../CommonUtilities/Others/daemon.c \
./Prefs/GetWord.c ./Prefs/Trim.c

CPPFILES = 	main.cpp \
			RunServer.cpp \
			QTSServer.cpp\
			QTSServerInterface.cpp \
			QTSSCallbacks.cpp \
			QTSSDictionary.cpp\
			QTSSDataConverter.cpp \
			QTSSMessages.cpp\
			QTSSModule.cpp \
			QTSSSocket.cpp\
			QTSSFile.cpp\
			QTSSUserProfile.cpp\
			QTSSExpirationDate.cpp\
			Prefs/QTSServerPrefs.cpp\
			Prefs/QTSSPrefs.cpp \
			Prefs/XMLPrefsParser.cpp \
			Prefs/XMLParser.cpp \
			Prefs/ConfParser.cpp \
			Prefs/FilePrefsSource.cpp \
			Prefs/GenerateXMLPrefs.cpp \
			SDP/SDPSourceInfo.cpp \
			SDP/SourceInfo.cpp \
			HTTP/HTTPProtocol.cpp \
			HTTP/HTTPRequest.cpp \
			RTSP/RTSPProtocol.cpp\
			RTSP/RTSPRequest.cpp \
			RTSP/RTSPRequestInterface.cpp\
			RTSP/RTSPRequestStream.cpp \
			RTSP/RTSPResponseStream.cpp\
			RTSP/RTSPSession.cpp \
			RTSP/RTSPSessionInterface.cpp\
			RTP/RTPSession.cpp \
			RTP/RTPSessionInterface.cpp\
			RTP/RTPStream.cpp \
			RTP/RTPPacketResender.cpp \
			RTP/RTPBandwidthTracker.cpp \
			RTP/RTPOverbufferWindow.cpp \
			RTP/RTPMetaInfoPacket.cpp\
			RTCP/RTCPTask.cpp\
			RTCP/RTCPAPPPacket.cpp\
			RTCP/RTCPPacket.cpp \
			RTCP/RTCPSRPacket.cpp\
			RTCP/RTCPAckPacket.cpp\
			../APIModules/APIStubLib/QTSS_Private.cpp \
			../APIModules/APICommonCode/QTSSModuleUtils.cpp\
			../APIModules/APICommonCode/QTSSRollingLog.cpp \
			../APIModules/APICommonCode/QTAccessFile.cpp \
			../APIModules/QTSSErrorLogModule/QTSSErrorLogModule.cpp\
			../APIModules/QTSSAccessLogModule/QTSSAccessLogModule.cpp \
			../APIModules/QTSSFileModule/QTSSFileModule.cpp \
			../APIModules/QTSSFlowControlModule/QTSSFlowControlModule.cpp \
			../APIModules/QTSSPOSIXFileSysModule/QTSSPosixFileSysModule.cpp \
			../APIModules/QTSSRefMovieModule/QTSSRefMovieModule.cpp \
			../APIModules/QTSSHomeDirectoryModule/DirectoryInfo.cpp \
			../APIModules/QTSSHomeDirectoryModule/QTSSHomeDirectoryModule.cpp \
			../CommonUtilities/SafeStdLib/InternalStdLib.cpp \
			../CommonUtilities/OSUtilities/OSMemory.cpp
			
			
			
LIBFILES = 	../MP4Packetizer/libQTFileLib.a \
			../CommonUtilities/libCommonUtilitiesLib.a

all: DarwinStreamingServer

DarwinStreamingServer: $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)  $(LIBFILES)
	$(LINK) -o $@ $(CFILES:.c=.o) $(CPPFILES:.cpp=.o) $(COMPILER_FLAGS) $(LINKOPTS) $(LIBS) 

install: DarwinStreamingServer

clean:
	rm -f $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)

.SUFFIXES: .cpp .c .o

.cpp.o:
	$(C++) -c -o $*.o $(DEFINES) $(C++FLAGS) $*.cpp

.c.o:
	$(CC) -c -o $*.o $(DEFINES) $(CCFLAGS) $*.c

