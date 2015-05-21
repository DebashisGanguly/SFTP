/*
 * FileTransferController.c
 *
 *  Created on: 21 Mar 2015
 *      Author: Debashis Ganguly
 */

#include "../SFTP.h"

int isMatch(char *string, char *pattern) {
	//Base case checks
	if (!string || !pattern)
		return 0;
	if (*string == '\0' && *pattern == '\0')
		return 1;
	if (*string == '\0' || *pattern == '\0')
		return 0;

	//Actual comparisons
	if ((*string == *pattern) || (*pattern == '?'))
		return isMatch(string + 1, pattern + 1);
	if (*pattern == '*')
		return (isMatch(string + 1, pattern) || isMatch(string + 1, pattern + 1)
				|| isMatch(string, pattern + 1));
	return 0;
}

DArray *parseArgumentToFileList(char *arg) {
	DArray *fileList = DArray_create(sizeof(char) * PATH_MAX, 100);
	char *filename;

	/* get the first token */
	filename = strtok(arg, ARG_SEP);

	/* walk through other tokens */
	while (filename != NULL) {
		DArray_push(fileList, filename);

		filename = strtok(NULL, ARG_SEP);
	}

	return fileList;
}

DArray *getMatchingFilesInCurrentDirectory(char *arg) {
	DArray *finalFileList = DArray_create(sizeof(char) * PATH_MAX, 100);

	int i = 0;
	DArray *allFiles = getAllFilesInCurrentDirectory();
	int allFilesCount = DArray_count(allFiles);
	char *currentFile;

	int j = 0;
	DArray *parsedArgumentFiles = parseArgumentToFileList(arg);
	int parsedArgumentCount = DArray_count(parsedArgumentFiles);
	char *argumentFile;

	if (parsedArgumentCount == 0 || allFilesCount == 0)
		return NULL;

	for (; i < allFilesCount; i++) {
		currentFile = DArray_get(allFiles, i);

		j = 0;

		for (; j < parsedArgumentCount; j++) {
			argumentFile = DArray_get(parsedArgumentFiles, j);

			if (isMatch(currentFile, argumentFile))
				DArray_push(finalFileList, currentFile);
		}
	}

	return finalFileList;
}

int canWriteFile(char *fileName) {
	struct stat st;

	if (stat(fileName, &st) < 0) {
		return 1;
	}

	if (st.st_mode & S_IWUSR)
		return 0;
	else
		return -1;
}

int establishGetDataConnectionToFS(int localControlSocket,
		char *localIPv4Address, char *localDataPort,
		NodeAddress *fsPrivateAddress, sftpCtrlCommand cmdId, char *arg) {
	int i = 0;
	int success = 0;

	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	fd_set fds;
	int n;
	struct timeval tv;

	int16_t responseMessageType, responseMessageMode, responseControlId;

	struct sockaddr_in fsSocketAddr;

	fsSocketAddr.sin_family = AF_INET;
	fsSocketAddr.sin_port = atoi(fsPrivateAddress->portNumber);
	inet_pton(AF_INET, fsPrivateAddress->ip, &(fsSocketAddr.sin_addr.s_addr));

	char argument[1024];

	strcpy(argument, localIPv4Address);
	strcat(argument, "-");
	strcat(argument, localDataPort);
	strcat(argument, "-");
	strcat(argument, arg);

	for (; i < numberOfAttempt && success == 0; i++) {
		packetSize = pack(buf, "hhhs", (int16_t) estDataConn, (int16_t) req,
				(int16_t) cmdId, argument);

		if (sendto(localControlSocket, buf, packetSize, 0,
				(struct sockaddr *) &fsSocketAddr,
				sizeof(fsSocketAddr)) == SOCKET_ERROR) {
			continue;
		}

		bzero(buf, sizeof(buf));

		FD_ZERO(&fds);
		FD_SET(localControlSocket, &fds);

		tv.tv_sec = 0;
		tv.tv_usec = timeOutusec;

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

			unpack(buf, "hhh", &responseMessageType, &responseMessageMode,
					&responseControlId);

			if (responseMessageType == estDataConn
					&& responseMessageMode == ack) {
				success = 1;
			}
			if (responseMessageType == estDataConn
					&& responseMessageMode == nack) {
				success = -1;
			}
		}
	}

	return success;
}

int establishPutDataConnectionToFS(int localControlSocket,
		NodeAddress *fsPrivateAddress, sftpCtrlCommand cmdId,
		struct sockaddr_in *serverDataSocketAddress) {
	int i = 0;
	int success = 0;

	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	fd_set fds;
	int n;
	struct timeval tv;

	int16_t responseMessageType, responseMessageMode, responseControlId;
	char responseArg[BUFSIZ];

	struct sockaddr_in fsSocketAddr;

	fsSocketAddr.sin_family = AF_INET;
	fsSocketAddr.sin_port = atoi(fsPrivateAddress->portNumber);
	inet_pton(AF_INET, fsPrivateAddress->ip, &(fsSocketAddr.sin_addr.s_addr));

	for (; i < numberOfAttempt && success == 0; i++) {
		packetSize = pack(buf, "hhhs", (int16_t) estDataConn, (int16_t) req,
				(int16_t) cmdId, "");

		if (sendto(localControlSocket, buf, packetSize, 0,
				(struct sockaddr *) &fsSocketAddr,
				sizeof(fsSocketAddr)) == SOCKET_ERROR) {
			continue;
		}

		bzero(buf, sizeof(buf));

		FD_ZERO(&fds);
		FD_SET(localControlSocket, &fds);

		tv.tv_sec = 0;
		tv.tv_usec = timeOutusec;

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

			unpack(buf, "hhh1024s", &responseMessageType, &responseMessageMode,
					&responseControlId, responseArg);

			if (responseMessageType == estDataConn
					&& responseMessageMode == ack) {
				success = 1;

				char *line = malloc(sizeof(char) * 1024);
				strcpy(line, responseArg);

				struct NodeAddress *serverDataChannelAddress = NULL;

				serverDataChannelAddress = malloc(sizeof(NodeAddress));
				line = strtok(line, NSINFO_LINE_SEP);
				serverDataChannelAddress->ip = malloc(sizeof(char) * 20);
				strcpy(serverDataChannelAddress->ip, line);
				line = strtok(NULL, NSINFO_LINE_SEP);
				serverDataChannelAddress->portNumber = malloc(
						sizeof(char) * 20);
				strcpy(serverDataChannelAddress->portNumber, line);

				serverDataSocketAddress->sin_family = AF_INET;
				serverDataSocketAddress->sin_port = atoi(
						serverDataChannelAddress->portNumber);
				inet_pton(AF_INET, serverDataChannelAddress->ip,
						&(serverDataSocketAddress->sin_addr.s_addr));
			}
		}
	}

	return success;
}

int disconnectDataConnection(int localControlSocket, NodeAddress *remoteAddress) {
	int i = 0;
	int success = 0;

	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	fd_set fds;
	int n;
	struct timeval tv;

	int16_t responseMessageType, responseMessageMode;

	struct sockaddr_in remoteSocketAddr;

	remoteSocketAddr.sin_family = AF_INET;
	remoteSocketAddr.sin_port = atoi(remoteAddress->portNumber);
	inet_pton(AF_INET, remoteAddress->ip, &(remoteSocketAddr.sin_addr.s_addr));

	for (; i < numberOfAttempt && success == 0; i++) {
		packetSize = pack(buf, "hh", (int16_t) terDataConn, (int16_t) req);

		if (sendto(localControlSocket, buf, packetSize, 0,
				(struct sockaddr *) &remoteSocketAddr,
				sizeof(remoteSocketAddr)) == SOCKET_ERROR) {
			continue;
		}

		bzero(buf, sizeof(buf));

		FD_ZERO(&fds);
		FD_SET(localControlSocket, &fds);

		tv.tv_sec = 0;
		tv.tv_usec = timeOutusec * 30;

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

			if (responseMessageType == terDataConn
					&& responseMessageMode == ack) {
				success = 1;
			}
		}
	}

	return success;
}

int sendFileTransferRequestPut(int controlSocket, NodeAddress *remoteAddress,
		char *fileName) {
	int i = 0;
	int success = -2;

	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	int16_t responseMessageType, responseMessageMode, responseMessageArg;

	struct sockaddr_in remoteSocketAddr;

	remoteSocketAddr.sin_family = AF_INET;
	remoteSocketAddr.sin_port = atoi(remoteAddress->portNumber);
	inet_pton(AF_INET, remoteAddress->ip, &(remoteSocketAddr.sin_addr.s_addr));

	for (; i < numberOfAttempt && success == -2; i++) {
		packetSize = pack(buf, "hhs", (int16_t) sendFile, (int16_t) req,
				fileName);
		if (sendto(controlSocket, buf, packetSize, 0,
				(struct sockaddr *) &remoteSocketAddr,
				sizeof(remoteSocketAddr)) == SOCKET_ERROR) {
			continue;
		}

		bzero(buf, sizeof(buf));

		if (read(controlSocket, buf,
		MAX_BUF_LEN) == SOCKET_ERROR) {
			continue;
		}

		unpack(buf, "hhh", &responseMessageType, &responseMessageMode,
				&responseMessageArg);

		if (responseMessageType == sendFile && responseMessageMode == ack) {
			success = responseMessageArg;
		}
	}

	return success;
}

int sendFileTransferRequestGet(int controlSocket, NodeAddress *remoteAddress,
		char *fileName) {
	int i = 0;
	int success = -2;

	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	int16_t responseMessageType, responseMessageMode, responseMessageArg;

	struct sockaddr_in remoteSocketAddr;

	remoteSocketAddr.sin_family = AF_INET;
	remoteSocketAddr.sin_port = atoi(remoteAddress->portNumber);
	inet_pton(AF_INET, remoteAddress->ip, &(remoteSocketAddr.sin_addr.s_addr));

	for (; i < numberOfAttempt && success == -2; i++) {
		packetSize = pack(buf, "hhs", (int16_t) sendFile, (int16_t) req,
				fileName);
		if (sendto(controlSocket, buf, packetSize, 0,
				(struct sockaddr *) &remoteSocketAddr,
				sizeof(remoteSocketAddr)) == SOCKET_ERROR) {
			continue;
		}

		bzero(buf, sizeof(buf));

		if (read(controlSocket, buf,
		MAX_BUF_LEN) == SOCKET_ERROR) {
			continue;
		}

		unpack(buf, "hhh", &responseMessageType, &responseMessageMode,
				&responseMessageArg);

		if (responseMessageType == sendFile && responseMessageMode == ack) {
			success = responseMessageArg;
		}
	}

	return success;
}

int startFileTransfer(int controlSocket, NodeAddress *remoteAddress) {
	int i = 0;
	int success = 0;

	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	fd_set fds;
	int n;
	struct timeval tv;

	int16_t responseMessageType, responseMessageMode;

	struct sockaddr_in remoteSocketAddr;

	remoteSocketAddr.sin_family = AF_INET;
	remoteSocketAddr.sin_port = atoi(remoteAddress->portNumber);
	inet_pton(AF_INET, remoteAddress->ip, &(remoteSocketAddr.sin_addr.s_addr));

	for (; i < numberOfAttempt && success == 0; i++) {
		packetSize = pack(buf, "hh", (int16_t) startTransfer, (int16_t) req);

		if (sendto(controlSocket, buf, packetSize, 0,
				(struct sockaddr *) &remoteSocketAddr,
				sizeof(remoteSocketAddr)) == SOCKET_ERROR) {
			continue;
		}

		bzero(buf, sizeof(buf));

		FD_ZERO(&fds);
		FD_SET(controlSocket, &fds);

		tv.tv_sec = 0;
		tv.tv_usec = timeOutusec;

		n = select(controlSocket + 1, &fds, NULL, NULL, &tv);
		if (n == 0)
			printf("Timeout occurred to receive response...\r\n"); // timeout!
		else if (n == -1)
			printf("Error occurred to select while receiving response...\r\n"); // error
		else {
			if (read(controlSocket, buf,
			MAX_BUF_LEN) == SOCKET_ERROR) {
				continue;
			}

			unpack(buf, "hh", &responseMessageType, &responseMessageMode);

			if (responseMessageType == startTransfer
					&& responseMessageMode == ack) {
				success = 1;
			}
		}
	}

	return success;
}
