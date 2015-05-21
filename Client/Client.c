/*
 * Client.c
 *
 *  Created on: 10-Mar-2015
 *      Author: Debashis Ganguly
 */

#include "../Global/SFTP.h"

void loopCommandExecution(void);

void establishControlConnectionWithFS();
int resolveFSFromNS(struct NodeAddress *ns);
int sendConnectionEstablishmentPacketToFS();
int sendSFTPControlPacketToFS(messageType mType, sftpCtrlCommand cmdId,
		char *arg);
void closeConnectionToFS();
void deleteNSFromList(int pos);

void printSystemHelp();

void execLocalPresentWorkingDirectory();
void execLocalChangeDirectory(char *path);
void execLocalDirectoryListing();

void execRemotePresentWorkingDirectory();
void execRemoteChangeDirectory(char *path);
void execRemoteDirectoryListing();

void handlePutSender(sftpCtrlCommand cmd, char *arg);
void handleGetReceiver(sftpCtrlCommand cmd, char *arg);

char *localIPv4Address;
int localControlSocket;
char *curDir;

struct NodeAddress *fsListenerAddress = NULL;
struct NodeAddress *fsPrivateAddress = NULL;

DArray *nsList;
int nsCount;

int connectFSStatus = 0;

int main(int argc, char** argv) {
	validateAndPopulateConfigurationItems();

	localIPv4Address = getIPv4Address();
	localControlSocket = createNetworkChannelSender();

	if (localControlSocket == SOCKET_ERROR) {
		return -1;
	}

	if (argc > 1) {
		if (strcmp(sftpLogicalName, argv[1]) == 0) {
			establishControlConnectionWithFS();
		}
	}

	loopCommandExecution();

	return 0;
}

void loopCommandExecution(void) {
	char line[1024];
	char *cmd;
	char *arg;
	char *tmp;

	puts("_____________________\r\n");
	printf("Welcome to SFTP!!!\r\n");
	puts("_____________________\r\n");

	for (;;) {
		/* Read a line of text */
		printf("SFTP> ");
		if (!fgets(line, sizeof(line), stdin) || *line == '\n') {
			printf("\n");
			continue;
		}

		/* Determine the argument of the command */
		cmd = line;
		arg = line;
		/* Find the first space */
		while (*arg && *arg > ' ')
			arg++;

		*arg = 0;
		arg++;
		/* Skip separators*/
		while (*arg && *arg <= ' ')
			arg++;
		tmp = arg;
		/* if we do not meet separator */
		while (*tmp && *tmp >= ' ')
			tmp++;
		*tmp = 0;

		/* Test command */
		if (strcmp(cmd, "quit") == 0) {
			if (connectFSStatus)
				closeConnectionToFS();
			close(localControlSocket);
			printf("Thank you for using SFTP...\r\n");
			exit(0);
		} else if (strcmp(cmd, "close") == 0) {
			if (connectFSStatus) {
				closeConnectionToFS();
				connectFSStatus = 0;
				printf("Connection to the server is terminated...\r\n");
			} else
				printf("You are not yet connected to the server...\r\n");
		} else if (strcmp(cmd, "?") == 0)
			printSystemHelp();
		else if (strcmp(cmd, "open") == 0) {
			if (connectFSStatus) {
				printf("You are already connected to the server...\r\n");
			} else {
				if (strcmp(sftpLogicalName, arg) == 0)
					establishControlConnectionWithFS();
				else
					printf(
							"open: you must provide the logical name of the file server.\r\n");
			}
		} else if (strcmp(cmd, "lpwd") == 0)
			execLocalPresentWorkingDirectory();
		else if (strcmp(cmd, "rpwd") == 0) {
			if (connectFSStatus)
				execRemotePresentWorkingDirectory();
			else
				printf("You are not yet connected to the server...\r\n");
		} else if (strcmp(cmd, "lcd") == 0) {
			if (*arg)
				execLocalChangeDirectory(arg);
			else
				printf("lcd: you must specify a directory.\r\n");
		} else if (strcmp(cmd, "rcd") == 0) {
			if (connectFSStatus) {
				if (*arg)
					execRemoteChangeDirectory(arg);
				else
					printf("rcd: you must specify a directory.\r\n");
			} else
				printf("You are not yet connected to the server...\r\n");
		} else if (strcmp(cmd, "lls") == 0) {
			execLocalDirectoryListing();
		} else if (strcmp(cmd, "rls") == 0) {
			if (connectFSStatus)
				execRemoteDirectoryListing();
			else
				printf("You are not yet connected to the server...\r\n");
		} else if (strcmp(cmd, "get") == 0) {
			if (*arg) {
				if (strchr(arg, '*') != NULL || strchr(arg, '?') != NULL)
					printf("get: invalid character in file name.\r\n");
				else {
					if (connectFSStatus)
						handleGetReceiver(get, arg);
					else
						printf(
								"You are not yet connected to the server...\r\n");
				}
			} else
				printf("get: you must specify a file.\r\n");
		} else if (strcmp(cmd, "mget") == 0) {
			if (*arg) {
				if (connectFSStatus)
					handleGetReceiver(mget, arg);
				else
					printf("You are not yet connected to the server...\r\n");
			} else
				printf("mget: you must specify a file.\r\n");
		} else if (strcmp(cmd, "put") == 0) {
			if (*arg) {
				if (strchr(arg, '*') != NULL || strchr(arg, '?') != NULL)
					printf("put: invalid character in file name.\r\n");
				else {
					if (connectFSStatus)
						handlePutSender(put, arg);
					else
						printf(
								"You are not yet connected to the server...\r\n");
				}
			} else
				printf("put: you must specify a file.\r\n");
		} else if (strcmp(cmd, "mput") == 0) {
			if (*arg) {
				if (connectFSStatus)
					handlePutSender(mput, arg);
				else
					printf("You are not yet connected to the server...\r\n");
			} else
				printf("mput: you must specify a file.\r\n");
		} else {
			printf("Bad Command: %s\r\n", cmd);
		}
	}
}

void closeConnectionToFS() {
	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	packetSize = pack(buf, "hh", (int16_t) disconnectFS, (int16_t) req);

	struct sockaddr_in fsSocketAddr;

	fsSocketAddr.sin_family = AF_INET;
	fsSocketAddr.sin_port = atoi(fsPrivateAddress->portNumber);
	inet_pton(AF_INET, fsPrivateAddress->ip, &(fsSocketAddr.sin_addr.s_addr));

	printf("Disconnecting connection to file server...\r\n");

	sendto(localControlSocket, buf, packetSize, 0,
			(struct sockaddr *) &fsSocketAddr, sizeof(fsSocketAddr));
}

void establishControlConnectionWithFS() {
	nsList = readNSInfo();
	nsCount = DArray_count(nsList);

	int randomNSPos;
	struct NodeAddress *ns = malloc(sizeof(NodeAddress));

	int resolveStatus = 0;

	if (nsCount == 0) {
		printf("There is no name server available currently...\r\n");
	} else {
		while (nsCount > 0 && connectFSStatus == 0) {
			if (resolveStatus == 0) {
				randomNSPos = getRandomNSPosition(nsCount);

				ns = DArray_get(nsList, randomNSPos);

				printf("Obtained name server info - %s:%s...\r\n", ns->ip,
						ns->portNumber);
			}

			while (connectFSStatus == 0) {
				resolveStatus = resolveFSFromNS(ns);

				if (resolveStatus) {
					printf("File server name is resolved successfully...\r\n");
					connectFSStatus = sendConnectionEstablishmentPacketToFS();

					if (connectFSStatus) {
						printf(
								"Established control connection to file server (%s:%s) successfully...\r\n",
								fsPrivateAddress->ip,
								fsPrivateAddress->portNumber);
					} else {
						printf(
								"Established control connection to file server failed...\r\n");
					}

				} else {
					printf("Resolving file server name failed...\r\n");
					deleteNSFromList(randomNSPos);
					break;
				}
			}
		}
	}
}

int resolveFSFromNS(struct NodeAddress *ns) {
	int i = 0;
	int success = 0;

	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	fd_set fds;
	int n;
	struct timeval tv;

	int16_t responseMessageType, responseMessageMode;

	struct sockaddr_in nsSocketAddr;

	nsSocketAddr.sin_family = AF_INET;
	nsSocketAddr.sin_port = atoi(ns->portNumber);
	inet_pton(AF_INET, ns->ip, &(nsSocketAddr.sin_addr.s_addr));

	for (; i < numberOfAttempt && success == 0; i++) {
		printf(
				"Requesting name Server to resolve file server... Attempt: %d\r\n",
				i + 1);

		packetSize = pack(buf, "hh", (int16_t) resolveFS, (int16_t) req);

		if (sendto(localControlSocket, buf, packetSize, 0,
				(struct sockaddr *) &nsSocketAddr,
				sizeof(nsSocketAddr)) == SOCKET_ERROR) {
			fsListenerAddress = NULL;
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

			char addressString[1024];

			unpack(buf, "hhs", &responseMessageType, &responseMessageMode,
					addressString);

			if (responseMessageType == resolveFS
					&& responseMessageMode == ack) {
				char *line = malloc(sizeof(char) * 1024);
				strcpy(line, addressString);

				fsListenerAddress = malloc(sizeof(NodeAddress));
				line = strtok(line, NSINFO_LINE_SEP);
				fsListenerAddress->ip = malloc(sizeof(char) * 20);
				strcpy(fsListenerAddress->ip, line);
				line = strtok(NULL, NSINFO_LINE_SEP);
				fsListenerAddress->portNumber = malloc(sizeof(char) * 20);
				strcpy(fsListenerAddress->portNumber, line);

				success = 1;
			} else {
				fsListenerAddress = NULL;
			}
		}
	}

	return success;
}

int sendConnectionEstablishmentPacketToFS() {
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
	fsSocketAddr.sin_port = atoi(fsListenerAddress->portNumber);
	inet_pton(AF_INET, fsListenerAddress->ip, &(fsSocketAddr.sin_addr.s_addr));

	for (; i < numberOfAttempt && success == 0; i++) {
		printf(
				"Requesting file server to open control channel... Attempt: %d\r\n",
				i + 1);

		packetSize = pack(buf, "hh", (int16_t) connectFS, (int16_t) req);

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

			char responseArg[MAX_BUF_LEN];
			char addressString[1024];

			unpack(buf, "hh1024s", &responseMessageType, &responseMessageMode,
					addressString);

			char *line = malloc(sizeof(char) * 1024);
			strcpy(line, addressString);

			fsPrivateAddress = malloc(sizeof(NodeAddress));
			line = strtok(line, NSINFO_LINE_SEP);
			fsPrivateAddress->ip = malloc(sizeof(char) * 20);
			strcpy(fsPrivateAddress->ip, line);
			line = strtok(NULL, NSINFO_LINE_SEP);
			fsPrivateAddress->portNumber = malloc(sizeof(char) * 20);
			strcpy(fsPrivateAddress->portNumber, line);

			if (responseMessageType == connectFS
					&& responseMessageMode == ack) {
				success = 1;
			} else {
				fsPrivateAddress = NULL;
			}
		}
	}

	return success;
}

int sendSFTPControlPacketToFS(messageType mType, sftpCtrlCommand cmdId,
		char *arg) {
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

	for (; i < numberOfAttempt && success == 0; i++) {
		if (cmdId == rcd) {
			packetSize = pack(buf, "hhhs", (int16_t) mType, (int16_t) req,
					(int16_t) cmdId, arg);
		} else {
			packetSize = pack(buf, "hhh", (int16_t) mType, (int16_t) req,
					(int16_t) cmdId);
		}

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

			char responseArg[MAX_BUF_LEN];

			unpack(buf, "hhh1024s", &responseMessageType, &responseMessageMode,
					&responseControlId, responseArg);

			if (responseMessageType == sftpCmd && responseMessageMode == ack) {
				if (responseControlId == rcd) {
					int success = atoi(responseArg);

					if (success == 0)
						printf("rcd: Successful...\r\n");
					else
						printf("rcd: Failed...\r\n");
				} else if (responseControlId == rls) {
					printf(
							"Directory listing of current directory at file server...\r\n");

					printf("%s", responseArg);
					printf("\r\n");
				} else if (responseControlId == rpwd) {
					printf("Current directory at file server: %s\r\n",
							responseArg);
				}
				success = 1;
			}
		}
	}

	return success;
}

void printSystemHelp() {
	printf(
			"SFTP Commands:\n"
					"___________________________\n"
					"\n"
					"?...............Help manual\n"
					"open <server> ..Connect to the file server <server>\n"
					"lpwd............Display the current local directory\n"
					"rpwd............Display the remote local directory\n"
					"lcd <dir>.......Go to the local directory <dir>\n"
					"rcd <dir>.......Go to the remote directory <dir>\n"
					"lls.............Display the contents of the current local directory\n"
					"rls.............Display the contents of the current remote directory\n"
					"get <file>......Get file <file>\n"
					"mget <file>.....Get multiple files <file>\n"
					"put <file>......Send the file <file>\n"
					"mput <file>.....Send multiple files <file>\n"
					"close...........Close connection with the file server\n"
					"quit............Exit SFTP application\n"
					"\n");
}

void execLocalPresentWorkingDirectory() {
	curDir = getCurrentWorkingDirectory();
	printf("\r\nCurrent directory: %s\r\n", curDir);
}

void execLocalChangeDirectory(char *path) {
	int success = changeDirectory(path);

	if (success == 0) {
		curDir = getCurrentWorkingDirectory();
		printf("\r\nLocal directory changed successfully...\r\n");
	} else {
		printf("\r\nChanging local directory failed...\r\n");
	}
}

void execLocalDirectoryListing() {
	char directoryListBuf[BUFSIZ];
	int listCount = getDirectoryListing(directoryListBuf,
			sizeof(directoryListBuf));

	printf("\r\nTotal count: %d\r\n", listCount);
	printf("\r\n%s\r\n", directoryListBuf);
}

void execRemotePresentWorkingDirectory() {
	sendSFTPControlPacketToFS(sftpCmd, rpwd, NULL);
}

void execRemoteChangeDirectory(char *path) {
	sendSFTPControlPacketToFS(sftpCmd, rcd, path);
}

void execRemoteDirectoryListing() {
	sendSFTPControlPacketToFS(sftpCmd, rls, NULL);
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

void handlePutSender(sftpCtrlCommand cmd, char *arg) {
	DArray *matchingFileList = getMatchingFilesInCurrentDirectory(arg);
	int matchingFileCount = 0;

	if (matchingFileList == NULL || (matchingFileCount = DArray_count(matchingFileList)) == 0) {
		printf(
				"No such file present in local directory or do not have read permission...\r\n");
	} else {
		char localDataPort[100];
		int dataChannelSocket = createNetworkChannelReceiver(localDataPort);

		struct sockaddr_in serverDataSocketAddress;
		int dataConnectionEstablishmentStatus = establishPutDataConnectionToFS(
				localControlSocket, fsPrivateAddress, cmd,
				&serverDataSocketAddress);

		printf("Created data connection with %s:%s\r\n", localIPv4Address,
				localDataPort);

		if (dataConnectionEstablishmentStatus) {
			int i = 0;
			char *fileName;
			char response = 'y';

			for (; i < matchingFileCount; i++) {
				fileName = DArray_get(matchingFileList, i);

				int transferRequestStatus = sendFileTransferRequestPut(
						localControlSocket, fsPrivateAddress, fileName);

				if (transferRequestStatus == 0) {
					printf(
							"File %s already exists in remote machine. Do you want to overwrite? Say (y)es or (n)o: ",
							fileName);

					scanf(" %c", &response);

					if (response != 'y' && response != 'Y')
						continue;

					int startTransferStatus = startFileTransfer(
							localControlSocket, fsPrivateAddress);

					if (startTransferStatus == 0)
						continue;

					printf("Transferring file %s to the remote machine...\r\n",
							fileName);

					dataFileSender(dataChannelSocket, serverDataSocketAddress,
							fileName);
				} else if (transferRequestStatus == -1) {
					printf(
							"File %s already exists in remote machine and cannot be overwritten...\r\n",
							fileName);
					continue;
				} else if (transferRequestStatus == 1) {
					int startTransferStatus = startFileTransfer(
							localControlSocket, fsPrivateAddress);

					if (startTransferStatus == 0)
						continue;

					printf("Transferring file %s to the remote machine...\r\n",
							fileName);

					dataFileSender(dataChannelSocket, serverDataSocketAddress,
							fileName);
				}
			}
		}

		int dataConnectionDisconnectStatus = disconnectDataConnection(
				localControlSocket, fsPrivateAddress);

		if (dataConnectionDisconnectStatus)
			close(dataChannelSocket);
	}
}

void handleGetReceiver(sftpCtrlCommand cmd, char *arg) {
	char localDataPort[100];
	int dataChannelSocket = createNetworkChannelReceiver(localDataPort);

	int dataConnectionEstablishmentStatus = establishGetDataConnectionToFS(
			localControlSocket, localIPv4Address, localDataPort,
			fsPrivateAddress, cmd, arg);

	if (dataConnectionEstablishmentStatus == 1) {
		printf("Created data connection with %s:%s\r\n", localIPv4Address,
				localDataPort);
		int packetSize;
		char buf[MAX_BUF_LEN];
		memset(buf, 0, sizeof(buf));
		struct sockaddr_in fsSocketAddr;

		fsSocketAddr.sin_family = AF_INET;
		fsSocketAddr.sin_port = atoi(fsPrivateAddress->portNumber);
		inet_pton(AF_INET, fsPrivateAddress->ip,
				&(fsSocketAddr.sin_addr.s_addr));

		socklen_t slen = sizeof(fsSocketAddr);

		int disconnectionRequest = 0;
		char fileName[BUFSIZ];

		int16_t requestMessageType, requestMessageMode;
		char arg[BUFSIZ];

		while (disconnectionRequest == 0) {
			bzero(buf, sizeof(buf));

			if (recvfrom(localControlSocket, buf, MAX_BUF_LEN, 0,
					(struct sockaddr*) &fsSocketAddr, &slen) == SOCKET_ERROR) {
				perror("FS: Error in receiving control packet...\r\n");
			}

			printf("Received packet to service...\r\n");

			unpack(buf, "hh1024s", &requestMessageType, &requestMessageMode,
					arg);

			if (requestMessageType == terDataConn
					&& requestMessageMode == req) {
				packetSize = pack(buf, "hh", (int16_t) terDataConn,
						(int16_t) ack);

				printf(
						"Serviced data connection termination request from %s:%s\r\n",
						fsPrivateAddress->ip, fsPrivateAddress->portNumber);

				if (sendto(localControlSocket, buf, packetSize, 0,
						(struct sockaddr *) &fsSocketAddr,
						sizeof(fsSocketAddr)) == SOCKET_ERROR) {
					perror("FS: Error in sending control packet...\r\n");
				}

				close(dataChannelSocket);

				disconnectionRequest = 1;
			}

			if (requestMessageType == sendFile && requestMessageMode == req) {
				strcpy(fileName, arg);

				int fileStatus = canWriteFile(fileName);
				int16_t sendFileStatus = 0;

				char response = 'y';

				if (fileStatus == 0) {
					printf(
							"File %s already exists in local machine. Do you want to overwrite? Say (y)es or (n)o: ",
							fileName);

					scanf(" %c", &response);

					if (response == 'y' || response == 'Y')
						sendFileStatus = 1;

				} else if (fileStatus == -1) {
					printf(
							"File %s already exists in local machine and cannot be overwritten...\r\n",
							fileName);
				} else if (fileStatus == 1) {
					sendFileStatus = 1;
				}

				packetSize = pack(buf, "hhh", (int16_t) sendFile, (int16_t) ack,
						sendFileStatus);

				printf("Serviced send file request from %s:%s\r\n",
						fsPrivateAddress->ip, fsPrivateAddress->portNumber);

				if (sendto(localControlSocket, buf, packetSize, 0,
						(struct sockaddr *) &fsSocketAddr,
						sizeof(fsSocketAddr)) == SOCKET_ERROR) {
					perror("FS: Error in sending control packet...\r\n");
				}
			}

			if (requestMessageType == startTransfer
					&& requestMessageMode == req) {
				packetSize = pack(buf, "hh", (int16_t) startTransfer,
						(int16_t) ack);

				printf("Serviced start transfer request from %s:%s\r\n",
						fsPrivateAddress->ip, fsPrivateAddress->portNumber);

				if (sendto(localControlSocket, buf, packetSize, 0,
						(struct sockaddr *) &fsSocketAddr,
						sizeof(fsSocketAddr)) == SOCKET_ERROR) {
					perror("FS: Error in sending control packet...\r\n");
				}

				printf("Receiving file %s from the remote machine...\r\n",
						fileName);

				dataFileReceiver(dataChannelSocket, fileName);
			}
		}
	} else {
		if (dataConnectionEstablishmentStatus == -1)
			printf(
					"No matching file present in remote machine to transfer...\r\n");
		close(dataChannelSocket);
	}
}
