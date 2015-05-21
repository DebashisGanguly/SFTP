/*
 * FileDataSender.c
 *
 *  Created on: 21 Mar 2015
 *      Author: Debashis Ganguly
 */

#include "../SFTP.h"

int fillWindow(char *fileName, int windowSize, DataPacket window[windowSize],
		int unacknowledgedPacketNumber, int *currentPacketRead, long *fileSize);
void sendWindow(int dataSocket, struct sockaddr_in remoteAddress,
		int windowSize, DataPacket window[windowSize],
		int unacknowledgedPacketNumber);
void receiveAcknowledgement(int dataSocket, int16_t *messageType,
		int16_t *messageArgument);
void sendFinAck(int dataSocket, struct sockaddr_in remoteAddress);

void dataFileSender(int dataSocket, struct sockaddr_in remoteAddress,
		char fileName[MAX_LINE]) {
	DataPacket window[windowSize];
	int unacknowledgedPacketNumber = 1, lastUnacknowledgedPacketNumber = 0;

	int endOfFile = 0, finReceived = 0, finalAckReceived = 0, eofAcknowledged =
			0;
	int currentPacketRead = 0;

	int expectedAckSequenceNumber = windowSize - 1;
	int receivedAckSequenceNumber;

	int windowRetransmitCount = 0;

	fd_set fds;
	int n;
	struct timeval tv;

	/*************************
	 * Statistical Analysis
	 */
	struct timeval startTimer, endTimer;
	gettimeofday(&startTimer, NULL);

	srand((unsigned) time(NULL));

	double elapsedTime = 0;
	long fileSize = 0;
	/*************************/

	while (eofAcknowledged == 0 || finalAckReceived == 0) {
		if (eofAcknowledged == 0) {
			if (endOfFile == 0)
				endOfFile = fillWindow(fileName, windowSize, window,
						unacknowledgedPacketNumber, &currentPacketRead,
						&fileSize);

			sendWindow(dataSocket, remoteAddress, windowSize, window,
					unacknowledgedPacketNumber);
		}

		FD_ZERO(&fds);
		FD_SET(dataSocket, &fds);

		tv.tv_sec = 0;
		tv.tv_usec = timeOutusec;

		n = select(dataSocket + 1, &fds, NULL, NULL, &tv);
		if (n == 0)
			; //printf("Timeout occurred to receive response...\r\n"); // timeout!
		else if (n == -1)
			; //printf("Error occurred to select while receiving response...\r\n"); // error
		else {
			int16_t messageType, messageArgument;

			receiveAcknowledgement(dataSocket, &messageType, &messageArgument);

			if (messageType == dataPacket) {
				receivedAckSequenceNumber = messageArgument;

				if (expectedAckSequenceNumber < receivedAckSequenceNumber)
					unacknowledgedPacketNumber += -1 + receivedAckSequenceNumber
							- expectedAckSequenceNumber;
				else
					unacknowledgedPacketNumber += windowSize - 1
							+ receivedAckSequenceNumber
							- expectedAckSequenceNumber;
				expectedAckSequenceNumber = receivedAckSequenceNumber - 1;
				if (expectedAckSequenceNumber < 0)
					expectedAckSequenceNumber = windowSize - 1;

				if (endOfFile == 1
						&& (unacknowledgedPacketNumber == currentPacketRead + 1))
					eofAcknowledged = 1;
			} else if (messageType == finMessage) {
				if (messageArgument == fin && finReceived == 0) {
					finReceived = 1;
					sendFinAck(dataSocket, remoteAddress);
				}
				if (messageArgument == finalAck)
					finalAckReceived = 1;
			}
		}

		if (lastUnacknowledgedPacketNumber == unacknowledgedPacketNumber) {
			windowRetransmitCount++;
			if (windowRetransmitCount == numberOfAttempt * 10) {
				printf(
						"Congested and error prone channel. Please try again later...\r\n");
				break;
			}
		} else {
			windowRetransmitCount = 0;

			lastUnacknowledgedPacketNumber = unacknowledgedPacketNumber;
		}
	}

	if (eofAcknowledged == 1) {
		gettimeofday(&endTimer, NULL);
		elapsedTime = (endTimer.tv_sec - startTimer.tv_sec) * 1000
				+ (endTimer.tv_usec - startTimer.tv_usec) / 1000;
		if (injErrorTest == 1)
			writeStatAnalysisLog(fileName, fileSize, elapsedTime);

		if (unacknowledgedPacketNumber == currentPacketRead + 1)
			printf("File %s sent successfully...\r\n", fileName);
	}
}

int fillWindow(char *fileName, int windowSize, DataPacket *window,
		int unacknowledgedPacketNumber, int *currentPacketRead, long *fileSize) {
	int endOfFile = 0;

	int8_t *buffer = malloc(mtu * sizeof(int8_t));
	int16_t readSize = 0;

	FILE *fp = fopen(fileName, "r");
	fseek(fp, mtu * (*currentPacketRead), SEEK_SET);

	int maxSequenceNumber = windowSize - 2;

	int i = (*currentPacketRead - unacknowledgedPacketNumber + 1);
	int currentWindowPointer = (*currentPacketRead) % windowSize;

	for (; i <= maxSequenceNumber; i++) {
		bzero(buffer, mtu);
		readSize = fread(buffer, 1, mtu, fp);

		(*fileSize) += readSize;

		DataPacket newPacket;

		newPacket.dataLength = readSize;
		newPacket.sequenceNumber = currentWindowPointer;
		if (readSize < mtu)
			newPacket.fragmentationFlag = 0;
		else
			newPacket.fragmentationFlag = 1;

		newPacket.data = malloc(sizeof(int8_t) * readSize);
		memcpy(newPacket.data, buffer, readSize);
		newPacket.crc = crc16(newPacket.dataLength, newPacket.data);

		window[currentWindowPointer] = newPacket;

		(*currentPacketRead)++;
		currentWindowPointer++;

		if (currentWindowPointer >= windowSize)
			currentWindowPointer %= windowSize;

		if (readSize < mtu) {
			endOfFile = 1;
			break;
		}
	}

	fclose(fp);

	return endOfFile;
}

void sendWindow(int dataSocket, struct sockaddr_in remoteAddress,
		int windowSize, DataPacket window[windowSize],
		int unacknowledgedPacketNumber) {
	/*************************
	 * Statistical Analysis
	 */
	int bytePositionToCorrupt;

	int bitErrorIntroduced = 0;
	int8_t *corruptedData;
	/*************************/

	int maxSequenceNumber = windowSize - 2;

	int i = 0;
	int currentWindowPointer = (unacknowledgedPacketNumber - 1) % windowSize;

	int packetSize;
	char buf[MAX_BUF_LEN];

	DataPacket newPacket;
	for (; i <= maxSequenceNumber; i++) {
		newPacket = window[currentWindowPointer];

		bzero(buf, sizeof(buf));

		char crcString[100];
		sprintf(crcString, "%d", newPacket.crc);

		char formatString[50];
		sprintf(formatString, "hhhhs%ua", newPacket.dataLength);

		//Introduce bit error based on probability of bit error
		if (injErrorTest == 1
				&& (bitErrorProbabiliy <= 1 && bitErrorProbabiliy >= 0)) {
			if (getRandomValueForAnalysis() < bitErrorProbabiliy
					&& newPacket.dataLength != 0) {
				bitErrorIntroduced = 1;
				corruptedData = malloc(sizeof(int8_t) * newPacket.dataLength);
				memcpy(corruptedData, newPacket.data, newPacket.dataLength);

				bytePositionToCorrupt = rand() % newPacket.dataLength;
				corruptedData[bytePositionToCorrupt] =
						(corruptedData[bytePositionToCorrupt] + 1) % 255;
			}
		}

		if (bitErrorIntroduced) {
			bitErrorIntroduced = 0;
			packetSize = pack(buf, formatString, dataPacket,
					newPacket.sequenceNumber, newPacket.fragmentationFlag,
					newPacket.dataLength, crcString, corruptedData);
		} else {
			packetSize = pack(buf, formatString, dataPacket,
					newPacket.sequenceNumber, newPacket.fragmentationFlag,
					newPacket.dataLength, crcString, newPacket.data);
		}

		//Introduce packet drop based on congestion percentage
		if (injErrorTest == 1
				&& (congestionPercentage <= 100 && congestionPercentage >= 0)) {
			if (getRandomValueForAnalysis() * 100 < congestionPercentage) {
				currentWindowPointer++;
				if (currentWindowPointer >= windowSize)
					currentWindowPointer %= windowSize;
				continue;
			}
		}

		if (sendto(dataSocket, buf, packetSize, 0,
				(struct sockaddr *) &remoteAddress,
				sizeof(remoteAddress)) == SOCKET_ERROR) {
			perror("Error in sending packet...\r\n");
		} else {
			//printf("Sent packet %d\r\n", newPacket.sequenceNumber);

			currentWindowPointer++;
			if (currentWindowPointer >= windowSize)
				currentWindowPointer %= windowSize;

			if (newPacket.fragmentationFlag == 0)
				break;
		}
	}
}

void receiveAcknowledgement(int dataSocket, int16_t *messageType,
		int16_t *messageArgument) {
	char buf[MAX_BUF_LEN];
	bzero(buf, sizeof(buf));

	struct sockaddr_in remoteAddress;
	socklen_t slen = sizeof(remoteAddress);

	if (recvfrom(dataSocket, buf, MAX_BUF_LEN, 0,
			(struct sockaddr*) &remoteAddress, &slen) == SOCKET_ERROR) {
		perror("Error in receiving acknowledgement...\r\n");
	} else {
		unpack(buf, "hh", messageType, messageArgument);

		/*
		 if ((*messageType) == dataPacket)
		 printf("Received acknowledgement sequence number %d\r\n",
		 *messageArgument);
		 if ((*messageType) == finMessage) {
		 if ((*messageArgument) == fin)
		 printf("Received fin\r\n");
		 if ((*messageArgument) == finalAck)
		 printf("Received final ack\r\n");
		 }
		 */
	}
}

void sendFinAck(int dataSocket, struct sockaddr_in remoteAddress) {
	int packetSize;
	char buf[MAX_BUF_LEN];

	packetSize = pack(buf, "hh", (int16_t) finMessage, (int16_t) finAck);

	if (sendto(dataSocket, buf, packetSize, 0,
			(struct sockaddr *) &remoteAddress,
			sizeof(remoteAddress)) == SOCKET_ERROR) {
		perror("Error in sending fin messages...\r\n");
	} else {
		//printf("Sent fin ack\r\n");
	}
}
