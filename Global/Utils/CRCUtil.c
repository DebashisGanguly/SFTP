/*
 * CRCUtil.c
 *
 *  Created on: 28 Mar 2015
 *      Author: Debashis Ganguly
 */

#include <stdio.h>
#include <stdlib.h>

#define POLY 0x8408

u_int16_t crc16(unsigned short length, int8_t data_p[length]) {
	unsigned char i;
	u_int16_t data;
	u_int16_t crc = 0xffff;

	if (length == 0)
		return (~crc);

	do {
		for (i = 0, data = (unsigned int) 0xff & *data_p++; i < 8; i++, data >>=
				1) {
			if ((crc & 0x0001) ^ (data & 0x0001))
				crc = (crc >> 1) ^ POLY;
			else
				crc >>= 1;
		}
	} while (--length);

	crc = ~crc;
	data = crc;
	crc = (crc << 8) | (data >> 8 & 0xff);

	return (crc);
}
