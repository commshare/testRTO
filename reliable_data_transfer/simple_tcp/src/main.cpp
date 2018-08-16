/* main.cpp
* CSCE 612-600 Spring 2017 
* HW3 reliable data transfer
* by Mian Qin
*/

#include "stdafx.h"
#include "SenderSocket.h"
#include "checksum.h"
//#include "vld.h"

void print_usage() {
	printf("Usage : reliable_data_transfer.exe hostname/IP \\ \n");
	printf("\t power-of-2 buffer size to be transmitted (in DWORDs) \\ \n");
	printf("\t sender window (in packets) \\ \n");
	printf("\t round-trip propagation delay (in seconds) \\ \n");
	printf("\t probability of loss in forward direction \\ \n");
	printf("\t probability of loss in return direction \\ \n");
	printf("\t speed of the bottleneck link (in Mbps)\n");
	printf("Example : reliable_data_transfer.exe s8.irl.cs.tamu.edu 24 50000 0.2 0.00001 0.0001 100 \n");
}

//Initialize WinSock
bool InitialWinsock()
{
	WSADATA wsaData;

	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return false;
	}
	return true;
}

void CleanWinsock()
{
	WSACleanup();
}

int main(int argc, char* argv[])
{
	int ret;
	clock_t time_elapse;

	/* initial winsock */
	if ((ret = InitialWinsock()) == false)
		return 0;

	/* check input argument */
	if (argc != 8) 
	{
		print_usage();
		return 0;
	}
	/* parse command-line parameters */
	char *targetHost = argv[1];
	int power = atoi(argv[2]);      // command-line specified integer 
	int senderWindow = atoi(argv[3]);     // command-line specified integer 
	LinkProperties lp;
	lp.RTT = atof(argv[4]);
	lp.speed = 1e6 * atof(argv[7]);        // convert to megabits 
	lp.pLoss[FORWARD_PATH] = atof(argv[5]);
	lp.pLoss[RETURN_PATH] = atof(argv[6]);
	printf("Main:\tsender W = %d, RTT %.3f sec, loss %g / %g, link %d Mbps\n", senderWindow, lp.RTT, lp.pLoss[FORWARD_PATH], lp.pLoss[RETURN_PATH], (int)(lp.speed/1e6));

	time_elapse = clock();
	UINT64 dwordBufSize = (UINT64)1 << power;
	DWORD *dwordBuf = new DWORD[dwordBufSize];    // user-requested buffer 
	for (UINT64 i = 0; i < dwordBufSize; i++)    // required initialization 
		dwordBuf[i] = i;
	printf("Main:\tinitializing DWORD array with 2^%d elements... done in %d ms\n", power,clock()- time_elapse);
	SenderSocket ss;
	
	if ((ret = ss.Open(targetHost, MAGIC_PORT, senderWindow, &lp)) != STATUS_OK)
	{
		printf("Main:\tconnect failed with status %d\n",ret);
		/* Cleanning Winsock */
		CleanWinsock();
		/* Cleanning Buffer*/
		delete[] dwordBuf;
		return 0;
	}
	printf("Main:\tconnected to %s in %.3f sec, pkt size %d bytes\n", targetHost, (float)(ss.EstimateRTT)/1000, MAX_PKT_SIZE);

	char *charBuf = (char*)dwordBuf;      // this buffer goes into socket 
	UINT64 byteBufferSize = dwordBufSize << 2;    // convert to bytes 
	
	// timer for duration
	time_elapse = clock();
	UINT64 off = 0;          // current position in buffer 
	while (off < byteBufferSize)
	{
		// decide the size of next chunk 
		int bytes = min(byteBufferSize - off, MAX_PKT_SIZE - sizeof(SenderDataHeader));
		// send chunk into socket 
		if ((ret = ss.Send(charBuf + off, bytes)) != STATUS_OK)
		{
			printf("Main:\tsend failed with status %d\n", ret);
			/* Cleanning Winsock */
			CleanWinsock();
			/* Cleanning Buffer*/
			delete[] dwordBuf;
			return 0;
		}
		off += bytes;
	}

	if ((ret = ss.Close(&time_elapse)) != STATUS_OK)
	{
		printf("Main:\tclose failed with status %d\n", ret);
		/* Cleanning Winsock */
		CleanWinsock();
		/* Cleanning Buffer*/
		delete[] dwordBuf;
		return 0;
	}

	Checksum cs;
	DWORD check = cs.CRC32((unsigned char *)charBuf, byteBufferSize);
	printf("Main:\ttransfer finished in %.3f sec, %.2f Kbps, checksum: %x \n",
		(float)(time_elapse) / 1000,
		(float) byteBufferSize * 8 / 1000 / ((float)(time_elapse) / 1000),
		check);
	printf("Main:\testRTT %.3f, ideal rate %.3f Kbps \n", 
		(float) ss.EstimateRTT / 1000,
		(float) ss.W * (MAX_PKT_SIZE - sizeof(SenderDataHeader)) * 8 / 1000 / ((float)ss.EstimateRTT / 1000)
		);
	/* Cleanning Winsock */
	CleanWinsock();

	/* Cleanning Buffer*/
	delete[] dwordBuf;

	return 0;
}
