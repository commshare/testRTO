/* SenderSocket.cpp
* CSCE 613 - 600 Spring 2017
* HW2 dns_server
* by Mian Qin
*/

/***  Acknowledgment   ***/
/*
* Use the program structure on piazza provied by the instructor
*
*/

#include "SenderSocket.h"

void SenderSocket::SenderHeaderConstructor(char *sendBuf, HeaderType type, LinkProperties *lp, DWORD seq)
{
	Flags flag;
	if (type == SYN)
	{
		flag.SYN = 1;
	}
	else
	{
		flag.FIN = 1;
	}
	SenderSynHeader *ssh = (SenderSynHeader *)sendBuf;

	ssh->sdh.flags = flag;
	ssh->sdh.seq = seq;
	ssh->lp = *lp;
}

void SenderSocket::SenderDataConstrutor(Packet *pkt, char *buf, int len, DWORD seq)
{
	Flags flag;
	SenderDataHeader *sdh = (SenderDataHeader *)(pkt->data);

	pkt->size = len + sizeof(SenderDataHeader);
	sdh->flags = flag;
	sdh->seq = seq;
	memcpy((char *)(pkt->data) + sizeof(SenderDataHeader), buf, len);
	pkt->resendFlag = false;
}

int SenderSocket::Open(char *targetHost, USHORT port, int senderWindow, LinkProperties *lp)
{
	int count = 0;
	int ret;
	clock_t send_timer;

	time_elapse = clock();
	(*lp).bufferSize = senderWindow + SYN_MAX_ATTEMPTS;
	linkP = lp;

	RTO = fmax(INIT_RTO, 2 * linkP->RTT);

	if (connected)
	{
		return ALREADY_CONNECTED;
	}
	
	// initial internal queue
	W = senderWindow;
	pkt = new Packet[W];

	// initial semaphore
	full = CreateSemaphore(
		NULL,           // default security attributes
		0,  // initial count
		senderWindow,  // maximum count
		NULL);          // unnamed semaphore
	empty = CreateSemaphore(
		NULL,           // default security attributes
		0,  // initial count
		senderWindow,  // maximum count
		NULL);          // unnamed semaphore

	// send buffer
	int pktSize = sizeof(SenderSynHeader);
	char *sendBuf = NULL;
	sendBuf = (char *)malloc(pktSize);

	// receive buffer
	ReceiverHeader rh;
	struct sockaddr_in response;
	int responseSize = sizeof(response);

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) {
		printf("[ %.3f] --> failed sockek init: %ld\n", ((float)(clock() - time_elapse)) / 1000, WSAGetLastError());
		free(sendBuf);
		return SOCKET_ERR;
	}
	int kernelBuffer = 200e6;     // 20 meg 
	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&kernelBuffer, sizeof(int)) == SOCKET_ERROR)
		printf("failed with setsockopt \n");
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&kernelBuffer, sizeof(int)) == SOCKET_ERROR)
		printf("failed with setsockopt \n");
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	u_long imode = 1;
	if (ioctlsocket(sock, FIONBIO, &imode) == SOCKET_ERROR)  // done once after creation 
		printf("failed with ioctlsocket \n");

	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(0);
	if (bind(sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR)
	{
		printf("[ %.3f] --> failed socket bind: %ld\n", ((float)(clock() - time_elapse)) / 1000, WSAGetLastError());
		free(sendBuf);
		return SOCKET_ERR;
	}
	// Doing DNS
	// structure used in DNS lookups
	struct hostent *hostname;
	struct in_addr addr;

	DWORD IP = inet_addr(targetHost);
	if (IP == INADDR_NONE)
	{
		// if not a valid IP, then do a DNS lookup
		if ((hostname = gethostbyname(targetHost)) == NULL)
		{
			free(sendBuf);
			printf("[ %.3f] --> target %s is invalid\n", ((float)(clock() - time_elapse)) / 1000, targetHost);
			return INVALID_NAME;
		}
		addr.s_addr = *(u_long *)hostname->h_addr_list[0];
	}
	else
	{
		addr.s_addr = inet_addr(targetHost);
	}

	remote.sin_family = AF_INET;
	remote.sin_addr.s_addr = addr.s_addr;
	remote.sin_port = htons(port);

	while (count++ < SYN_MAX_ATTEMPTS)
	{
		// construct sendsynheader
		SenderHeaderConstructor(sendBuf, SYN, linkP, 0);
			
		// send syn header packet
#ifdef DEBUG_ON
		printf("[ %.3f] --> SYN %d (attempt %d of %d, RTO %.3f) to %s\n", ((float)(clock() - time_elapse)) / 1000, 0, count, SYN_MAX_ATTEMPTS, RTO, inet_ntoa(addr));
#endif // DEBUG_ON

		send_timer = clock();
		if (sendto(sock, sendBuf, pktSize, 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR)
		{
			printf("[ %.3f] --> failed sendto: %ld\n", ((float)(clock() - time_elapse)) / 1000, WSAGetLastError());
			free(sendBuf);
			return FAILED_SEND;
		}		

		// receive ack packet
		// set timeout 
		timeval timeout = { (int)RTO, (int)((RTO - (int)RTO) * 1000000) };
		fd_set fd;
		FD_ZERO(&fd);       // clear the set 
		FD_SET(sock, &fd);   // add your socket to the set 

		int recvBytes;
		if ((ret = select(0, &fd, NULL, NULL, &timeout)) > 0)
		{
			recvBytes = recvfrom(sock, (char *)&rh, sizeof(ReceiverHeader), 0, (SOCKADDR *)&response, &responseSize);
			if (recvBytes == SOCKET_ERROR)
			{
				printf("[ %.3f] --> failed recvfrom: %ld\n", ((float)(clock() - time_elapse)) / 1000, WSAGetLastError());
				free(sendBuf);
				return FAILED_RECV;
			}
			EstimateRTO(clock() - send_timer);
#ifdef DEBUG_ON
			printf("[ %.3f] <-- SYN-ACK %d window %d; setting initial RTO to %.3f\n", ((double)(clock() - time_elapse)) / 1000, rh.ackSeq, rh.recvWnd, RTO);		
#endif // DEBUG_ON
			
			connected = true;
			free(sendBuf);

			// initialize empty slot
			lastReleased = min(senderWindow, rh.recvWnd);
			ReleaseSemaphore(empty, lastReleased, NULL);

			// initial ack thread
			ackThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadACK_Run, (void*) this, 0, NULL);
			// initial stats thread
			statsThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadStats_Run, (void*) this, 0, NULL);
			return STATUS_OK;
		}
	}

	free(sendBuf);
	return TIMEOUT;
}

int SenderSocket::SendOnePacket(char *buf, int len)
{
	int ret;
	while (sendto(sock, buf, len, 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR)
	{
		ret = WSAGetLastError();
		if (ret == WSAEWOULDBLOCK)
		{
			printf("******** WSAEWOULDBLOCK occurred ********\n");
			// wait on select 
			timeval timeout = { 10 , 0 };
			fd_set fd;
			FD_ZERO(&fd);       // clear the set 
			FD_SET(sock, &fd);   // add your socket to the set 
			// should not fail
			if ((ret = select(0, NULL, &fd, NULL, &timeout)) > 0)
			{
				continue;
			}
		}
		else
			return FAILED_SEND;
	}
	return STATUS_OK;
}

ReceiverHeader SenderSocket::ReceiveACK()
{
	int ret;
	// receive buffer
	ReceiverHeader rh;
	struct sockaddr_in response;
	int responseSize = sizeof(response);

	if (recvfrom(sock, (char *)&rh, sizeof(ReceiverHeader), 0, (SOCKADDR *)&response, &responseSize) == SOCKET_ERROR)
		printf("recvfrom error %d\n", WSAGetLastError());
	return rh;

}

int SenderSocket::Send(char *buf, int len)
{
	if (!connected)
	{
		return NOT_CONNECTED;
	}

	HANDLE events[] = { eventQuit, empty };
	DWORD result = WaitForMultipleObjects(2, events, false, INFINITE);
	if (result == WAIT_OBJECT_0)
	{
		return TIMEOUT;
	}
	else if (result == WAIT_OBJECT_0 + 1)
	{
		// send buffer
		SenderDataConstrutor(&pkt[pktQueueCount%W], buf, len, pktQueueCount);
		pktQueueCount++;
		ReleaseSemaphore(full, 1, NULL);
		return STATUS_OK;
	}
}

DWORD SenderSocket::ThreadACK()
{	
	LARGE_INTEGER dueTime;
	DWORD ack_seq;
	char dupAck;
	clock_t gtime_elapse = clock();
	ReceiverHeader rh;
	HANDLE eventTimeout = CreateWaitableTimer(NULL, TRUE, NULL);
	dueTime.QuadPart = ms2qp(INFINITE);
	SetWaitableTimer(eventTimeout, &dueTime, 0, NULL, NULL, 0);
	int newReleased;
	HANDLE events[] = { socketReceiveReady, full, eventClose, eventTimeout };

	bool closeflag = false;

	// TIME_CRITICAL
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	if (WSAEventSelect(sock, socketReceiveReady, FD_READ) == SOCKET_ERROR)
	{
		printf("WSAEventSelect error code: %d\n", WSAGetLastError());
	}

	while (true)
	{
		DWORD ret = WaitForMultipleObjects(4, events, false, INFINITE);
		switch (ret)
		{
		case WAIT_OBJECT_0 + 3:
			stats.timeoutCount++;
			if (++retx_count < DATA_MAX_ATTEMPTS)
			{
				// restart timer
				dueTime.QuadPart = ms2qp((long)(RTO * 1000));
				SetWaitableTimer(eventTimeout, &dueTime, 0, NULL, NULL, 0);
				SendOnePacket(pkt[sendBase%W].data, pkt[sendBase%W].size);
				pkt[sendBase%W].resendFlag = true;
#ifdef DEBUG_ON
				printf("[ %.3f] --> retx pkt %d (attempt %d of %d, RTO %.3f) \n", ((float)(clock() - gtime_elapse)) / 1000, sendBase, retx_count+1, DATA_MAX_ATTEMPTS, RTO);
#endif // DEBUG_ON
			}
			else
			{
				SetEvent(eventQuit);
				SetEvent(eventClose);
			}
			break;
		case WAIT_OBJECT_0:
			rh = ReceiveACK(); // move senderBase; fast retx
			ack_seq = rh.ackSeq;
			// move senderBase
			if (ack_seq > sendBase)
			{
				stats.totalAckBytes += (ack_seq - sendBase) * (MAX_PKT_SIZE + sizeof(SenderDataHeader));
				
				if (pkt[(ack_seq - 1)%W].resendFlag == false)
				{
					if (checkReTx(sendBase, ack_seq - 1))
					{
						//printf("base: %d, ack_seq-1: %d,",sendBase,ack_seq-1);
						EstimateRTO(clock() - pkt[(ack_seq - 1) % W].timeStamp);
						//printf("simpleRTT: %d, estimateRTT: %d, RTO: %.3f\n", clock() - pkt[(ack_seq - 1) % W].timeStamp,EstimateRTT, RTO);
					}
				}

				sendBase = ack_seq;
				if (seq > sendBase)
				{
					// reset timer
					dueTime.QuadPart = ms2qp((long)((RTO * 1000) - (clock() - pkt[sendBase%W].timeStamp)));
					SetWaitableTimer(eventTimeout, &dueTime, 0, NULL, NULL, 0);
					//printf("reset timer %d, clock %d ts %d sendBase %d\n", (long)((RTO * 1000) - (clock() - pkt[sendBase%W].timeStamp)), clock(), pkt[sendBase%W].timeStamp, sendBase);
				}

				effectiveWin = min(W, rh.recvWnd);

				// how much we can advance the semaphore 
				newReleased = sendBase + effectiveWin - lastReleased;
				ReleaseSemaphore(empty, newReleased, NULL);
				lastReleased += newReleased;
				retx_count = 0;
				dupAck = 0;
#ifdef DEBUG_ON
				printf("[ %.3f] <-- ACK %d window %d, RTT %.3f, RTO %.3f) \n", ((float)(clock() - gtime_elapse)) / 1000, ack_seq, rh.recvWnd, (float)EstimateRTT / 1000, RTO);
#endif // DEBUG_ON

				if (sendBase == seq)
				{
					dueTime.QuadPart = ms2qp(INFINITE);
					SetWaitableTimer(eventTimeout, &dueTime, 0, NULL, NULL, 0);
					if (closeflag)
					{
						SetEvent(eventAckall);
						return 0;
					}

				}
			}
			else if (ack_seq == sendBase)
			{
				// fast retx
				if (++dupAck == 3)
				{
					// reset timer
					dueTime.QuadPart = ms2qp((long)(RTO * 1000));
					SetWaitableTimer(eventTimeout, &dueTime, 0, NULL, NULL, 0);
					// resend base
					SendOnePacket(pkt[sendBase%W].data, pkt[sendBase%W].size);
					pkt[sendBase%W].resendFlag = true;
					retx_count++;
#ifdef DEBUG_ON
					printf("[ %.3f] --> fastretx pkt %d ts %d (attempt %d of %d, RTO %.3f) \n", ((float)(clock() - gtime_elapse)) / 1000, sendBase, clock(), retx_count+1, DATA_MAX_ATTEMPTS, RTO);
#endif // DEBUG_ON
					stats.fastretxCount++;
					//dupAck = 0;
				}
			}
			break;
		case WAIT_OBJECT_0 + 1:
			if (seq == sendBase)
			{
				dueTime.QuadPart = ms2qp((long)(RTO * 1000));
				SetWaitableTimer(eventTimeout, &dueTime, 0, NULL, NULL, 0);
			}
			pkt[seq%W].timeStamp = clock();
			SendOnePacket(pkt[seq%W].data, pkt[seq%W].size);
#ifdef DEBUG_ON
			printf("[ %.3f] --> pkt %d ts %d (attempt %d of %d, RTO %.3f) \n", ((float)(clock() - gtime_elapse)) / 1000, seq, clock(), 1, DATA_MAX_ATTEMPTS, RTO);
#endif // DEBUG_ON
			seq++;

			break;
		case WAIT_OBJECT_0 + 2:
			closeflag = true;
		default:
			break;
		}
	}
	return 0;
}

bool SenderSocket::checkReTx(int start, int end)
{
	for (int i = start; i <= end; i++)
	{
		if (pkt[i%W].resendFlag == true)
			return false;
	}
	return true;
}

void SenderSocket::EstimateRTO(int simple_rtt)
{
	if (EstimateRTT == 0)
		EstimateRTT = simple_rtt;
	EstimateRTT = (1 - alpha)*EstimateRTT + alpha * simple_rtt;
	DevRTT = (1 - beta)*DevRTT + beta * abs(simple_rtt - EstimateRTT);
	RTO = (float)(EstimateRTT + 4 * max(DevRTT, 10)) / 1000;
#ifdef DEBUG_ON
	printf("SimpleRTT: %d, EstimateRTT: %d, Estimate RTO: %f\n", simple_rtt, EstimateRTT, RTO);
#endif // DEBUG_ON
}

int SenderSocket::Close(clock_t *elapse)
{
	int count = 0;
	int ret;
	HANDLE events[] = { eventAckall, eventQuit };

	if (!connected)
	{
		return NOT_CONNECTED;
	}

	SetEvent(eventClose);
	WaitForMultipleObjects(2, events, false, INFINITE);
	*elapse = clock() - *elapse;
	
	WaitForSingleObject(ackThread, INFINITE);

	// send buffer
	int pktSize = sizeof(SenderSynHeader);
	char *sendBuf = NULL;
	sendBuf = (char *)malloc(pktSize);

	// receive buffer
	ReceiverHeader rh;
	struct sockaddr_in response;
	int responseSize = sizeof(response);

	while (count++ < FIN_MAX_ATTEMPTS)
	{
		// construct sendsynheader
		SenderHeaderConstructor(sendBuf, FIN, linkP, seq);
#ifdef DEBUG_ON
		// send syn header packet
		printf("[ %.3f] --> FIN %d (attempt %d of %d, RTO %.3f)\n", ((float)(clock() - time_elapse)) / 1000, seq, count, FIN_MAX_ATTEMPTS, RTO);
#endif // DEBUG_ON
		
		if (sendto(sock, sendBuf, pktSize, 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR)
		{
			printf("[ %.3f] --> failed sendto: %ld\n", ((float)(clock() - time_elapse)) / 1000, WSAGetLastError());
			free(sendBuf);
			return FAILED_SEND;
		}

		// receive ack packet
		// set timeout 
		timeval timeout = { (int)RTO, (int)((RTO - (int)RTO) * 1000000) };
		fd_set fd;
		FD_ZERO(&fd);       // clear the set 
		FD_SET(sock, &fd);   // add your socket to the set 

		int recvBytes;
		while (true)
		{
			if ((ret = select(0, &fd, NULL, NULL, &timeout)) > 0)
			{
				recvBytes = recvfrom(sock, (char *)&rh, sizeof(ReceiverHeader), 0, (SOCKADDR *)&response, &responseSize);
				if (recvBytes == SOCKET_ERROR)
				{
					printf("[ %.3f] --> failed recvfrom: %ld\n", ((float)(clock() - time_elapse)) / 1000, WSAGetLastError());
					free(sendBuf);
					return FAILED_RECV;
				}
				if (rh.flags.FIN == 1)
				{
					printf("[ %.3f] <-- FIN-ACK %d window %x\n", ((double)(clock() - time_elapse)) / 1000, rh.ackSeq, rh.recvWnd);

					free(sendBuf);
					closesocket(sock);
					return STATUS_OK;
				}
				else
					continue;
			}
			break;
		}
	}
	free(sendBuf);
	return TIMEOUT;
}

DWORD SenderSocket::ThreadStats()
{
	int tCount = 0;
	while (WaitForSingleObject(eventClose, STAT_PRINT_INTERVAL * 1000) == WAIT_TIMEOUT)
	{
		tCount++;
		// print 
		printf("[ %d] B\t%d (%.1f MB) N\t%d T %d F %d W %d S %.3f Mbps RTT %.3f \n",
			tCount * STAT_PRINT_INTERVAL,
			sendBase,
			(float)stats.totalAckBytes / 1024 / 1024,
			seq,
			stats.timeoutCount,
			stats.fastretxCount,
			effectiveWin,
			(float)(seq- stats.lastSeq) * 8 * (MAX_PKT_SIZE - sizeof(SenderDataHeader)) / 1000 / 1000 / STAT_PRINT_INTERVAL,
			(float)EstimateRTT / 1000
		);
		stats.lastSeq = seq;
	}
	SetEvent(eventClose);
	return 0;
}