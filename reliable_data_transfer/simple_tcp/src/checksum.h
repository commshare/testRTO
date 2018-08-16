/* checksum.h
* CSCE 612 - 600 Spring 2017
* HW3 reliable data transfer
* by Mian Qin
*/
#include "stdafx.h"

class Checksum
{
public:
	DWORD crc_table[256];

	Checksum();
	DWORD CRC32(unsigned char *buf, size_t len);
};
