# Copyright (c) 1999-2002 Apple Computer, Inc.  All rights reserved.
# modified by tao_627@yahoo.cn 
# last update 2011-07-02 

NAME = QTSSRefMovieModule
C++ = $(CPLUS)
CC = $(CCOMP)
LINK = $(LINKER)
CCFLAGS += $(COMPILER_FLAGS) $(INCLUDE_FLAG) ../../Build/PlatformHeader.h -g -Wall
LIBS = $(CORE_LINK_LIBS) $(SHARED) $(MODULE_LIBS) ../../CommonUtilities/libCommonUtilitiesLib.a ../../MP4Packetizer/libQTFileLib.a

#OPTIMIZATION
CCFLAGS += -O3

# EACH DIRECTORY WITH HEADERS MUST BE APPENDED IN THIS MANNER TO THE CCFLAGS

CCFLAGS += -I.
CCFLAGS += -I../APICommonCode
CCFLAGS += -I../OSMemory_Modules
CCFLAGS += -I../APIStubLib
CCFLAGS += -I../../CommonUtilities/SafeStdLib
CCFLAGS += -I../../CommonUtilities/Encrypt
CCFLAGS += -I../../CommonUtilities/OSUtilities
CCFLAGS += -I../../CommonUtilities/Others
CCFLAGS += -I../../CommonUtilities/Socket
CCFLAGS += -I../../CommonUtilities/String
CCFLAGS += -I../../CommonUtilities/Task
CCFLAGS += -I../../MP4Packetizer
CCFLAGS += -I../../ServerCore/RTP
CCFLAGS += -I../../ServerCore/SDP

# EACH DIRECTORY WITH A STATIC LIBRARY MUST BE APPENDED IN THIS MANNER TO THE LINKOPTS

LINKOPTS = -L../../CommonUtilities

C++FLAGS = $(CCFLAGS)

CPPFILES = 	../OSMemory_Modules/OSMemory_Modules.cpp \
			../APIStubLib/QTSS_Private.cpp \
			../APICommonCode/QTSSModuleUtils.cpp\
			../../ServerCore/SDP/SDPSourceInfo.cpp \
			../../ServerCore/SDP/SourceInfo.cpp \
			../../ServerCore/RTP/RTPMetaInfoPacket.cpp \
			QTSSRefMovieModule.cpp

LIBFILES = 	../../MP4Packetizer/libQTFileLib.a \
			../../CommonUtilities/libCommonUtilitiesLib.a

all: QTSSRefMovieModule

QTSSRefMovieModule: $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)  $(LIBFILES)
	$(LINK) -o $@ $(CFILES:.c=.o) $(CPPFILES:.cpp=.o) $(COMPILER_FLAGS) $(LINKOPTS) $(LIBS) 

install: QTSSRefMovieModule

clean:
	rm -f QTSSRefMovieModule $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)

.SUFFIXES: .cpp .c .o

.cpp.o:
	$(C++) -c -o $*.o $(DEFINES) $(C++FLAGS) $*.cpp

.c.o:
	$(CC) -c -o $*.o $(DEFINES) $(CCFLAGS) $*.c

