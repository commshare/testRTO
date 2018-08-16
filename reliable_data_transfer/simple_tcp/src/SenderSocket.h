/* SenderSocket.h
* CSCE 613 - 600 Spring 2017
* HW2 dns_server
* by Mian Qin
*/

/***  Acknowledgment   ***/
/*
* Part of define from homework pdf
*
*/

#ifndef   _sender_socket_h_   
#define   _sender_socket_h_

#include "stdafx.h"

//#define DEBUG_ON

typedef enum { SYN, FIN } HeaderType;

#define USHORT unsigned short 
#define SYN_MAX_ATTEMPTS 50
#define DATA_MAX_ATTEMPTS 50
#define FIN_MAX_ATTEMPTS 50
#define INIT_RTO 1

#define alpha 0.125
#define beta 0.25

#define MAGIC_PORT  22345    // receiver listens on this port 
#define MAX_PKT_SIZE  (1500-28)  // maximum UDP packet size accepted by receiver

// possible status codes from ss.Open, ss.Send, ss.Close 
#define STATUS_OK    0  // no error 
#define ALREADY_CONNECTED  1  // second call to ss.Open() without closing connection 
#define NOT_CONNECTED    2  // call to ss.Send()/Close() without ss.Open() 
#define INVALID_NAME    3  // ss.Open() with targetHost that has no DNS entry 
#define FAILED_SEND    4  // sendto() failed in kernel 
#define TIMEOUT    5  // timeout after all retx attempts are exhausted 
#define FAILED_RECV    6  // recvfrom() failed in kernel  
#define SOCKET_ERR	7 // kernel socket error

#define MAGIC_PROTOCOL 0x8311AA 

#define FORWARD_PATH    0 
#define RETURN_PATH    1 

#define STAT_PRINT_INTERVAL 2

#define ms2qp(t_ms) (-((LONGLONG)t_ms * 10000))
#pragma pack(push,1)     // sets struct padding/alignment to 1 byte 
class Flags {
public:
	DWORD      reserved : 5;      // must be zero   
	DWORD      SYN : 1;
	DWORD      ACK : 1;
	DWORD      FIN : 1;
	DWORD      magic : 24;

	Flags() { memset(this, 0, sizeof(*this)); magic = MAGIC_PROTOCOL; }
};

class SenderDataHeader {
public:
	Flags      flags;
	DWORD      seq;		// must begin from 0 
};

class ReceiverHeader {
public:
	Flags      flags;
	DWORD      recvWnd;  // receiver window for flow control (in pkts) 
	DWORD      ackSeq;   // ack value = next expected sequence 
};

class LinkProperties {
public:
	// transfer parameters 
	float      RTT;    // propagation RTT (in sec) 
	float      speed;    // bottleneck bandwidth (in bits/sec) 
	float      pLoss[2];  // probability of loss in each direction 
	DWORD      bufferSize;  // buffer size of emulated routers (in packets) 

	LinkProperties() { memset(this, 0, sizeof(*this)); }
};

class SenderSynHeader {
public:
	SenderDataHeader  sdh;
	LinkProperties   lp;
};
#pragma pack(pop)		// restores old packing

class Packet {
public:
	char data[MAX_PKT_SIZE];
	int size;
	clock_t timeStamp;
	bool resendFlag;
};

class Statistic
{
public:
	long long totalAckBytes;
	int timeoutCount;
	int fastretxCount;
	int lastSeq;

	Statistic()
	{
		totalAckBytes = 0;
		timeoutCount = 0;
		fastretxCount = 0;
		lastSeq = 0;
	}
};

class SenderSocket {
public:
	LinkProperties *linkP;
	clock_t time_elapse;
	float RTO; // unit: s
	int EstimateRTT; // unit: ms
	int DevRTT; // unit: ms
	SOCKET sock;
	struct sockaddr_in local;
	struct sockaddr_in remote;
	bool connected;

	bool pendingPkt;
	char retx_count;
	DWORD sendBase;
	DWORD seq;
	HANDLE eventQuit, eventClose, eventAckall;
	WSAEVENT socketReceiveReady;
	HANDLE full, empty;
	HANDLE ackThread;
	HANDLE statsThread;
	int W;
	Packet *pkt;
	unsigned int pktQueueCount;
	Statistic stats;

	int lastReleased, effectiveWin;

	SenderSocket() 
	{
		connected = false;
		memset(&local, 0, sizeof(local));
		memset(&remote, 0, sizeof(remote));

		EstimateRTT = 0;
		DevRTT = 0;
		retx_count = 0;
		sendBase = 0;
		seq = 0;
		pendingPkt = false;
		pktQueueCount = 0;

		//socketReceiveReady = WSACreateEvent();
		socketReceiveReady = CreateEvent(NULL, false, false, NULL);
		eventQuit = CreateEvent(NULL, true, false, NULL);
		eventClose = CreateEvent(NULL, false, false, NULL);
		eventAckall = CreateEvent(NULL, true, false, NULL);
	}

	~SenderSocket()
	{
		delete[] pkt;
	}

	int Open(char *targetHost, USHORT port, int senderWindow, LinkProperties *lp);
	int Send(char *buf, int len);
	int Close(clock_t *elapse);

	int SendOnePacket(char *buf, int len);
	ReceiverHeader ReceiveACK();

	void SenderHeaderConstructor(char *sendBuf, HeaderType type, LinkProperties *lp, DWORD seq);
	void SenderDataConstrutor(Packet *pkt, char *buf, int len, DWORD seq);

	bool checkReTx(int start, int end);
	void EstimateRTO(int current_rtt);

	DWORD ThreadACK();
	static DWORD WINAPI ThreadACK_Run(LPVOID p)
	{
		SenderSocket* This = (SenderSocket*)p;
		return This->ThreadACK();
	}
	DWORD ThreadStats();
	static DWORD WINAPI ThreadStats_Run(LPVOID p)
	{
		SenderSocket* This = (SenderSocket*)p;
		return This->ThreadStats();
	}
};

#endif