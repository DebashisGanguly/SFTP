/*
 * FileDataReceiver.c
 *
 *  Created on: 25 Mar 2015
 *      Author: Debashis Ganguly
 */

#include "../SFTP.h"

void writeWindow(char *fileName, DataPacket *window, int expectedSequenceNumber);
void sendAcknowledgement(int dataSocket, struct sockaddr_in remoteAddress,
		int16_t expectedSequenceNumber);
void sendFinMessages(int dataSocket, struct sockaddr_in remoteAddress,
		finMessageType finType);

void dataFileReceiver(int dataSocket, char fileName[MAX_LINE]) {
	remove(fileName);

	int endOfFile = 0, finSent = 0, finAckReceived = 0;

	int expectedSequenceNumber = 0;

	char buf[mtu + 100];
	int16_t messageType, finType, sequenceNumber, fragmentationFlag, dataLength;
	char crcString[100];
	u_int16_t crc;
	int8_t *data;

	DataPacket window[windowSize];

	struct sockaddr_in remoteAddress;
	socklen_t slen = sizeof(remoteAddress);

	int consecutivePacketCount = 0;

	int timeOutusecForTermination = timeOutusec * numberOfAttempt * 10;

	fd_set fds;
	int n;
	struct timeval tv;

	while (finAckReceived == 0) {
		bzero(buf, sizeof(buf));

		FD_ZERO(&fds);
		FD_SET(dataSocket, &fds);

		tv.tv_sec = 0;
		tv.tv_usec = timeOutusecForTermination;

		n = select(dataSocket + 1, &fds, NULL, NULL, &tv);
		if (n == 0)
			break;
		else if (n == -1)
			; //printf("Error occurred to select while receiving packets...\r\n"); // error
		else {
			if (recvfrom(dataSocket, buf, MAX_BUF_LEN, 0,
					(struct sockaddr*) &remoteAddress, &slen) == SOCKET_ERROR) {
				perror("Error in receiving data packet...\r\n");
				continue;
			}

			unpack(buf, "h", &messageType);

			if (messageType == dataPacket) {

				consecutivePacketCount++;

				DataPacket newPacket;

				int32_t packetSize = unpack((buf + 2), "hhh100s",
						&sequenceNumber, &fragmentationFlag, &dataLength,
						crcString);

				char formatString[10];
				sprintf(formatString, "%da", dataLength);

				data = malloc(sizeof(int8_t) * dataLength);

				unpack((buf + 2 + packetSize), formatString, data);

				crc = atoi(crcString);

				if (sequenceNumber == expectedSequenceNumber
						&& crc == crc16(dataLength, data)) {
					newPacket.sequenceNumber = sequenceNumber;
					newPacket.fragmentationFlag = fragmentationFlag;
					newPacket.crc = crc;
					newPacket.dataLength = dataLength;
					newPacket.data = malloc(sizeof(int8_t) * dataLength);
					memcpy(newPacket.data, data, dataLength);

					window[expectedSequenceNumber] = newPacket;
					expectedSequenceNumber++;

					//printf("Received packet %d\r\n", newPacket.sequenceNumber);

					if ((fragmentationFlag == 0
							|| expectedSequenceNumber >= windowSize)
							&& finSent == 0) {
						writeWindow(fileName, window, expectedSequenceNumber);
					}

					if (expectedSequenceNumber >= windowSize)
						expectedSequenceNumber %= windowSize;

					if (fragmentationFlag == 0)
						endOfFile = 1;
				}

				if (consecutivePacketCount == windowSize - 1 || endOfFile) {
					sendAcknowledgement(dataSocket, remoteAddress,
							expectedSequenceNumber);
					consecutivePacketCount = 0;
				}

				if (endOfFile == 1 && finSent == 0) {
					finSent = 1;
					sendFinMessages(dataSocket, remoteAddress, fin);
				}
			} else if (messageType == finMessage) {
				unpack((buf + 2), "h", &finType);

				if (finType == finAck) {
					finAckReceived = 1;
					//printf("Received fin ack\r\n");
					sendFinMessages(dataSocket, remoteAddress, finalAck);
				}
			}
		}
	}
	if (finAckReceived)
		printf("File %s received successfully...\r\n", fileName);
	else {
		printf("Timeout occurred to receive file...\r\n"); // timeout!
		remove(fileName);
	}
}

void writeWindow(char *fileName, DataPacket *window, int expectedSequenceNumber) {
	FILE *fp = fopen(fileName, "a+");

	int i = 0;

	for (; i < expectedSequenceNumber; i++) {
		DataPacket newPacket = window[i];

		fwrite(newPacket.data, newPacket.dataLength, 1, fp);
	}

	fclose(fp);
}

void sendAcknowledgement(int dataSocket, struct sockaddr_in remoteAddress,
		int16_t expectedSequenceNumber) {
	int packetSize;
	char buf[MAX_BUF_LEN];

	packetSize = pack(buf, "hh", dataPacket, expectedSequenceNumber);

	if (sendto(dataSocket, buf, packetSize, 0,
			(struct sockaddr *) &remoteAddress,
			sizeof(remoteAddress)) == SOCKET_ERROR) {
		perror("Error in sending acknowledgement...\r\n");
	} else {
		//printf("Sent ack %d\r\n", expectedSequenceNumber);
	}
}

void sendFinMessages(int dataSocket, struct sockaddr_in remoteAddress,
		finMessageType finType) {
	int packetSize;
	char buf[MAX_BUF_LEN];

	packetSize = pack(buf, "hh", (int16_t) finMessage, (int16_t) finType);

	if (sendto(dataSocket, buf, packetSize, 0,
			(struct sockaddr *) &remoteAddress,
			sizeof(remoteAddress)) == SOCKET_ERROR) {
		perror("Error in sending fin messages...\r\n");
	} /*else {
	 if (finType == fin)
	 printf("Sent fin\r\n");
	 if (finType == finalAck)
	 printf("Sent final ack\r\n");
	 }*/
}
