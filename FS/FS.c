/*
 * FS.c
 *
 *  Created on: 10-Mar-2015
 *      Author: Debashis Ganguly
 */

#include "../Global/SFTP.h"

int registertoNS(struct NodeAddress *ns);
void deleteNSFromList(int pos);
void serviceRequestLoop();
void serviceSpecificRequest(struct sockaddr_in senderAddress,
		int16_t requestMessageType, int16_t requestMessageMode);

char *localIPv4Address;
char localPort[100];
int localControlSocket;

DArray *nsList;
int nsCount;

void handlePutReceiver(int privateControlSocket,
		struct sockaddr_in senderSocketAddress, sftpCtrlCommand cmd);
void handleGetSender(int privateControlSocket,
		struct sockaddr_in senderSocketAddress, sftpCtrlCommand cmd,
		char *sftpArg);

int main(int argc, char** argv) {
	validateAndPopulateConfigurationItems();

	localIPv4Address = getIPv4Address();

	localControlSocket = createNetworkChannelReceiver(localPort);

	printf(
			"Bootstrapping file server...\r\nCreated control channel for %s:%s\r\n",
			localIPv4Address, localPort);

	nsList = readNSInfo();
	nsCount = DArray_count(nsList);

	int success = 0;

	while (nsCount > 0 && success == 0) {
		int randomNSPos = getRandomNSPosition(nsCount);

		struct NodeAddress *ns = DArray_get(nsList, randomNSPos);

		printf("Obtained name server info - %s:%s...\r\n", ns->ip,
				ns->portNumber);

		success = registertoNS(ns);

		if (success)
			printf("Registration to name server is successful...\r\n");
		else
			deleteNSFromList(randomNSPos);
	}

	if (success)
		serviceRequestLoop();
	else
		printf("Dying out as no name server available to register...\r\n");

	return 0;
}

int registertoNS(struct NodeAddress *ns) {
	int i = 0;
	int success = 0;

	int timeOutusecRegNS = timeOutusec * numberOfAttempt * nsCount;

	fd_set fds;
	int n;
	struct timeval tv;

	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	int16_t responseMessageType, responseMessageMode;

	struct sockaddr_in nsSocketAddr;

	nsSocketAddr.sin_family = AF_INET;
	nsSocketAddr.sin_port = atoi(ns->portNumber);
	inet_pton(AF_INET, ns->ip, &(nsSocketAddr.sin_addr.s_addr));

	char addressString[1024];

	strcpy(addressString, localIPv4Address);
	strcat(addressString, "-");
	strcat(addressString, localPort);

	for (; i < numberOfAttempt && success == 0; i++) {
		printf("Requesting Name Server to register... Attempt: %d\r\n", i + 1);

		packetSize = pack(buf, "hhs", (int16_t) registerFS, (int16_t) req,
				addressString);

		if (sendto(localControlSocket, buf, packetSize, 0,
				(struct sockaddr *) &nsSocketAddr,
				sizeof(nsSocketAddr)) == SOCKET_ERROR) {
			continue;
		}

		bzero(buf, sizeof(buf));

		FD_ZERO(&fds);
		FD_SET(localControlSocket, &fds);

		tv.tv_sec = 0;
		tv.tv_usec = timeOutusecRegNS;

		n = select(localControlSocket + 1, &fds, NULL, NULL, &tv);
		if (n == 0)
			printf("Timeout occurred to receive response...\r\n"); // timeout!
		else if (n == -1)
			printf("Error occurred to select while receiving response...\r\n"); // error
		else {
			if (read(localControlSocket, buf,
			MAX_BUF_LEN) == SOCKET_ERROR) {
				continue;
			}

			unpack(buf, "hh", &responseMessageType, &responseMessageMode);

			if (responseMessageType == registerFS && responseMessageMode == ack)
				success = 1;
		}
	}

	return success;
}

void deleteNSFromList(int pos) {
	DArray *tempList = DArray_create(sizeof(NodeAddress), 10);
	int i = 0;

	for (; i < nsCount; i++) {
		if (i != pos) {
			NodeAddress *oldNode = DArray_get(nsList, i);
			NodeAddress *newNode = DArray_new(tempList);
			newNode->ip = malloc(sizeof(char) * strlen(oldNode->ip));
			strcpy(newNode->ip, oldNode->ip);
			newNode->portNumber = malloc(
					sizeof(char) * strlen(oldNode->portNumber));
			strcpy(newNode->portNumber, oldNode->portNumber);
			DArray_push(tempList, newNode);
		}
	}

	DArray_clear_destroy(nsList);
	nsList = tempList;
	nsCount = DArray_count(nsList);
}

void serviceRequestLoop() {
	struct sockaddr_in senderAddress;
	socklen_t slen = sizeof(senderAddress);

	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	int16_t requestMessageType, requestMessageMode;

	while (1) {
		printf("Ready to listen...\r\n");

		bzero(buf, sizeof(buf));

		if (recvfrom(localControlSocket, buf, MAX_BUF_LEN, 0,
				(struct sockaddr*) &senderAddress, &slen) == SOCKET_ERROR) {
			perror("FS: Error in receiving control packet...\r\n");
		}

		printf("Received packet to service...\r\n");

		unpack(buf, "hh", &requestMessageType, &requestMessageMode);

		pid_t childProcess;

		childProcess = fork();

		if (childProcess == 0) {
			//child process
			close(localControlSocket);

			serviceSpecificRequest(senderAddress, requestMessageType,
					requestMessageMode);
		} else if (childProcess > 0) {
			//parent process
		} else {
			printf("Error creating child process to serve clients...\r\n");
		}
	}

	close(localControlSocket);
}

void serviceSpecificRequest(struct sockaddr_in senderSocketAddress,
		int16_t requestMessageType, int16_t requestMessageMode) {
	char *curDir;

	socklen_t slen = sizeof(senderSocketAddress);

	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	int sendResponse = 0;

	NodeAddress *senderAddress = malloc(sizeof(NodeAddress));

	senderAddress->ip = malloc(100 * sizeof(char));
	void *ptr = &senderSocketAddress.sin_addr;
	inet_ntop(AF_INET, ptr, senderAddress->ip, 100);
	senderAddress->portNumber = malloc(100 * sizeof(char));
	sprintf(senderAddress->portNumber, "%d", senderSocketAddress.sin_port);

	if (requestMessageType == connectFS && requestMessageMode == req) {

		char localPrivatePort[100];
		int privateControlSocket = createNetworkChannelReceiver(
				localPrivatePort);

		char addressString[1024];

		strcpy(addressString, localIPv4Address);
		strcat(addressString, "-");
		strcat(addressString, localPrivatePort);

		packetSize = pack(buf, "hhs", (int8_t) requestMessageType, (int8_t) ack,
				addressString);

		if (sendto(privateControlSocket, buf, packetSize, 0,
				(struct sockaddr *) &senderSocketAddress,
				sizeof(senderSocketAddress)) == SOCKET_ERROR) {
			perror("FS: Error in sending control packet...\r\n");
		}

		printf("Serviced control connection request from %s:%s\r\n",
				senderAddress->ip, senderAddress->portNumber);

		int sftpPacketSize;
		char sftpBuf[MAX_BUF_LEN];
		memset(sftpBuf, 0, sizeof(sftpBuf));

		int16_t sftpRequestMessageType, sftpRequestMessageMode,
				sftpRequestControlId;
		char sftpArg[BUFSIZ];

		while (1) {
			printf("Ready to listen to private port from client...\r\n");

			bzero(buf, sizeof(buf));

			if (recvfrom(privateControlSocket, buf, MAX_BUF_LEN, 0,
					(struct sockaddr*) &senderSocketAddress,
					&slen) == SOCKET_ERROR) {
				perror("FS: Error in receiving control packet...\r\n");
			}

			printf("Received packet to service...\r\n");

			unpack(buf, "hhh1024s", &sftpRequestMessageType,
					&sftpRequestMessageMode, &sftpRequestControlId, sftpArg);

			if (sftpRequestMessageType == sftpCmd
					&& sftpRequestMessageMode == req) {
				sendResponse = 1;

				if (sftpRequestControlId == rcd) {
					int success = changeDirectory(sftpArg);

					curDir = getCurrentWorkingDirectory();

					char rcdResponse[BUFSIZ];
					sprintf(rcdResponse, "%d", success);

					packetSize = pack(buf, "hhhs",
							(int16_t) sftpRequestMessageType, (int16_t) ack,
							(int16_t) sftpRequestControlId, rcdResponse);
				} else if (sftpRequestControlId == rls) {
					char directoryListBuf[BUFSIZ];
					getDirectoryListing(directoryListBuf,
							sizeof(directoryListBuf));

					packetSize = pack(buf, "hhhs",
							(int16_t) sftpRequestMessageType, (int16_t) ack,
							(int16_t) sftpRequestControlId, directoryListBuf);
				} else if (sftpRequestControlId == rpwd) {
					curDir = getCurrentWorkingDirectory();

					packetSize = pack(buf, "hhhs",
							(int16_t) sftpRequestMessageType, (int16_t) ack,
							(int16_t) sftpRequestControlId, curDir);
				}

				printf("Serviced SFTP control command request from %s:%s\r\n",
						senderAddress->ip, senderAddress->portNumber);

				if (sendResponse == 1) {
					if (sendto(privateControlSocket, buf, packetSize, 0,
							(struct sockaddr *) &senderSocketAddress,
							sizeof(senderSocketAddress)) == SOCKET_ERROR) {
						perror("FS: Error in sending control packet...\r\n");
					}
				}
			} else if (sftpRequestMessageType == disconnectFS
					&& sftpRequestMessageMode == req) {
				printf(
						"Serviced closing control connection request from %s:%s\r\n",
						senderAddress->ip, senderAddress->portNumber);
				close(privateControlSocket);
				exit(0);
			} else if (sftpRequestMessageType == estDataConn
					&& sftpRequestMessageMode == req) {
				if (sftpRequestControlId == get
						|| sftpRequestControlId == mget) {
					handleGetSender(privateControlSocket, senderSocketAddress,
							sftpRequestControlId, sftpArg);
				}
				if (sftpRequestControlId == put
						|| sftpRequestControlId == mput) {
					handlePutReceiver(privateControlSocket, senderSocketAddress,
							sftpRequestControlId);
				}
			}
		}
	}
}

void handlePutReceiver(int privateControlSocket,
		struct sockaddr_in senderSocketAddress, sftpCtrlCommand cmd) {
	socklen_t slen = sizeof(senderSocketAddress);

	NodeAddress *senderAddress = malloc(sizeof(NodeAddress));

	senderAddress->ip = malloc(100 * sizeof(char));
	void *ptr = &senderSocketAddress.sin_addr;
	inet_ntop(AF_INET, ptr, senderAddress->ip, 100);
	senderAddress->portNumber = malloc(100 * sizeof(char));
	sprintf(senderAddress->portNumber, "%d", senderSocketAddress.sin_port);

	printf("Received data connection establishment request from %s:%s\r\n",
			senderAddress->ip, senderAddress->portNumber);

	char localDataPort[100];
	int dataChannelSocket = createNetworkChannelReceiver(localDataPort);

	char addressString[1024];

	strcpy(addressString, localIPv4Address);
	strcat(addressString, "-");
	strcat(addressString, localDataPort);

	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	packetSize = pack(buf, "hhhs", (int16_t) estDataConn, (int16_t) ack,
			(int16_t) cmd, addressString);
	if (sendto(privateControlSocket, buf, packetSize, 0,
			(struct sockaddr *) &senderSocketAddress,
			sizeof(senderSocketAddress)) == SOCKET_ERROR) {
		perror("FS: Error in sending control packet...\r\n");
	}

	int disconnectionRequest = 0;
	char fileName[BUFSIZ];

	int16_t requestMessageType, requestMessageMode;
	char arg[BUFSIZ];

	while (disconnectionRequest == 0) {
		bzero(buf, sizeof(buf));

		if (recvfrom(privateControlSocket, buf, MAX_BUF_LEN, 0,
				(struct sockaddr*) &senderSocketAddress, &slen) == SOCKET_ERROR) {
			perror("FS-PS_Put: Error in receiving control packet...\r\n");
			exit(0);
		}

		printf("Received packet to service...\r\n");

		unpack(buf, "hh1024s", &requestMessageType, &requestMessageMode, arg);

		if (requestMessageType == terDataConn && requestMessageMode == req) {
			packetSize = pack(buf, "hh", (int16_t) terDataConn, (int16_t) ack);

			printf(
					"Serviced data connection termination request from %s:%s\r\n",
					senderAddress->ip, senderAddress->portNumber);

			if (sendto(privateControlSocket, buf, packetSize, 0,
					(struct sockaddr *) &senderSocketAddress,
					sizeof(senderSocketAddress)) == SOCKET_ERROR) {
				perror("FS: Error in sending control packet...\r\n");
			}

			close(dataChannelSocket);

			disconnectionRequest = 1;
		}

		if (requestMessageType == sendFile && requestMessageMode == req) {
			strcpy(fileName, arg);

			int16_t fileStatus = canWriteFile(fileName);

			packetSize = pack(buf, "hhh", (int16_t) sendFile, (int16_t) ack,
					fileStatus);

			printf("Serviced send file request from %s:%s\r\n",
					senderAddress->ip, senderAddress->portNumber);

			if (sendto(privateControlSocket, buf, packetSize, 0,
					(struct sockaddr *) &senderSocketAddress,
					sizeof(senderSocketAddress)) == SOCKET_ERROR) {
				perror("FS: Error in sending control packet...\r\n");
			}
		}

		if (requestMessageType == startTransfer && requestMessageMode == req) {
			packetSize = pack(buf, "hh", (int16_t) startTransfer,
					(int16_t) ack);

			printf("Serviced start transfer request from %s:%s\r\n",
					senderAddress->ip, senderAddress->portNumber);

			if (sendto(privateControlSocket, buf, packetSize, 0,
					(struct sockaddr *) &senderSocketAddress,
					sizeof(senderSocketAddress)) == SOCKET_ERROR) {
				perror("FS: Error in sending control packet...\r\n");
			}

			printf("Receiving file %s from the remote machine...\r\n",
					fileName);

			dataFileReceiver(dataChannelSocket, fileName);
		}
	}
}

void handleGetSender(int privateControlSocket,
		struct sockaddr_in senderSocketAddress, sftpCtrlCommand cmd,
		char *sftpArg) {
	int dataChannelSocket;

	NodeAddress *senderAddress = malloc(sizeof(NodeAddress));

	senderAddress->ip = malloc(100 * sizeof(char));
	void *ptr = &senderSocketAddress.sin_addr;
	inet_ntop(AF_INET, ptr, senderAddress->ip, 100);
	senderAddress->portNumber = malloc(100 * sizeof(char));
	sprintf(senderAddress->portNumber, "%d", senderSocketAddress.sin_port);

	char *line = malloc(sizeof(char) * 1024);
	strcpy(line, sftpArg);

	struct NodeAddress *clientDataChannelAddress = NULL;

	clientDataChannelAddress = malloc(sizeof(NodeAddress));
	line = strtok(line, NSINFO_LINE_SEP);
	clientDataChannelAddress->ip = malloc(sizeof(char) * 20);
	strcpy(clientDataChannelAddress->ip, line);
	line = strtok(NULL, NSINFO_LINE_SEP);
	clientDataChannelAddress->portNumber = malloc(sizeof(char) * 20);
	strcpy(clientDataChannelAddress->portNumber, line);
	line = strtok(NULL, NSINFO_LINE_SEP);

	struct sockaddr_in clientSocketAddress;

	clientSocketAddress.sin_family = AF_INET;
	clientSocketAddress.sin_port = atoi(clientDataChannelAddress->portNumber);
	inet_pton(AF_INET, clientDataChannelAddress->ip,
			&(clientSocketAddress.sin_addr.s_addr));

	DArray *matchingFileList = getMatchingFilesInCurrentDirectory(line);
	int matchingFileCount = 0;

	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	if (matchingFileList == NULL || (matchingFileCount = DArray_count(matchingFileList)) == 0) {
		packetSize = pack(buf, "hhh", (int16_t) estDataConn, (int16_t) nack,
				(int16_t) cmd);
		if (sendto(privateControlSocket, buf, packetSize, 0,
				(struct sockaddr *) &senderSocketAddress,
				sizeof(senderSocketAddress)) == SOCKET_ERROR) {
			perror("FS: Error in sending control packet...\r\n");
		}
		printf("No matching file present in remote machine to transfer...\r\n");
	} else {
		packetSize = pack(buf, "hhh", (int16_t) estDataConn, (int16_t) ack,
				(int16_t) cmd);
		if (sendto(privateControlSocket, buf, packetSize, 0,
				(struct sockaddr *) &senderSocketAddress,
				sizeof(senderSocketAddress)) == SOCKET_ERROR) {
			perror("FS: Error in sending control packet...\r\n");
		}

		dataChannelSocket = createNetworkChannelSender();

		int i = 0;
		char *fileName;

		for (; i < matchingFileCount; i++) {
			fileName = DArray_get(matchingFileList, i);

			int transferRequestStatus = sendFileTransferRequestGet(
					localControlSocket, senderAddress, fileName);

			if (transferRequestStatus == 1) {
				int startTransferStatus = startFileTransfer(localControlSocket,
						senderAddress);

				if (startTransferStatus == 0)
					continue;

				printf("Transferring file %s to the remote machine...\r\n",
						fileName);

				dataFileSender(dataChannelSocket, clientSocketAddress,
						fileName);
			}
		}

		int dataConnectionDisconnectStatus = disconnectDataConnection(
				localControlSocket, senderAddress);

		if (dataConnectionDisconnectStatus)
			close(dataChannelSocket);
	}
}
