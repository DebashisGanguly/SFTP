/*
 * NS.c
 *
 *  Created on: 09-Mar-2015
 *      Author: Debashis Ganguly
 *     Purpose:
 */

#include "../Global/SFTP.h"
#include "../Global/DArray.h"

DArray *nsList;
int nsCount = 0;
int nsId;

DArray *fsList;
int fsPosition = 0;

char *localIPv4Address;
char localPort[100];

int controlSocket;

void serviceRequestLoop();
void serviceSpecificRequest(struct sockaddr_in senderAddress,
		int16_t requestMessageType, int16_t requestMessageMode, char *arg);

void addFStoPool(NodeAddress *fsAddress);
NodeAddress *getFSFromServicePool();

void loopOnNeighbouringNSToRegister();
int registerToNS(NodeAddress *ns);
void addNSToList(NodeAddress *nsAddress);

void loopOnNeighbouringNSToSync();
int syncToNS(NodeAddress *ns, NodeAddress *fs);

int main(int argc, char** argv) {
	validateAndPopulateConfigurationItems();

	localIPv4Address = getIPv4Address();

	controlSocket = createNetworkChannelReceiver(localPort);

	printf(
			"Bootstrapping name server...\r\nCreated control channel for %s:%s\r\n",
			localIPv4Address, localPort);

	nsList = readNSInfo();
	nsId = DArray_count(nsList);
	nsCount = nsId;

	int pos = checkNSExists(nsList, localIPv4Address, localPort);

	if (pos == -1) {
		writeNSInfo(localIPv4Address, localPort, nsId);
	}

	NodeAddress *ownAddress = malloc(sizeof(NodeAddress));
	ownAddress->ip = malloc(sizeof(char) * 20);
	strcpy(ownAddress->ip, localIPv4Address);
	ownAddress->portNumber = malloc(sizeof(char) * 20);
	strcpy(ownAddress->portNumber, localPort);

	addNSToList(ownAddress);

	loopOnNeighbouringNSToRegister();

	fsList = DArray_create(sizeof(NodeAddress), 100);

	serviceRequestLoop();

	return 0;
}

void serviceRequestLoop() {
	struct sockaddr_in senderAddress;
	socklen_t slen = sizeof(senderAddress);

	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	int16_t requestMessageType, requestMessageMode;
	char arg[BUFSIZ];

	while (1) {
		printf("Ready to listen...\r\n");

		bzero(buf, sizeof(buf));

		if (recvfrom(controlSocket, buf, MAX_BUF_LEN, 0,
				(struct sockaddr*) &senderAddress, &slen) == SOCKET_ERROR) {
			perror("NS: Error in receiving control packet...\r\n");
		}

		printf("Received packet to service...\r\n");

		unpack(buf, "hh1024s", &requestMessageType, &requestMessageMode, arg);
		serviceSpecificRequest(senderAddress, requestMessageType,
				requestMessageMode, arg);
	}
}

void serviceSpecificRequest(struct sockaddr_in senderAddress,
		int16_t requestMessageType, int16_t requestMessageMode, char *arg) {
	int sendResponse = 0;

	int packetSize;
	char buf[MAX_BUF_LEN];
	memset(buf, 0, sizeof(buf));

	if (requestMessageType == registerFS && requestMessageMode == req) {
		packetSize = pack(buf, "hh", (int16_t) requestMessageType,
				(int16_t) ack);

		NodeAddress *fsAddress = malloc(sizeof(NodeAddress));
		arg = strtok(arg, NSINFO_LINE_SEP);
		fsAddress->ip = malloc(sizeof(char) * 20);
		strcpy(fsAddress->ip, arg);
		arg = strtok(NULL, NSINFO_LINE_SEP);
		fsAddress->portNumber = malloc(sizeof(char) * 20);
		strcpy(fsAddress->portNumber, arg);

		addFStoPool(fsAddress);

		printf("Serviced register file server request from %s:%s\r\n",
				fsAddress->ip, fsAddress->portNumber);

		if (nsCount > 1) {
			loopOnNeighbouringNSToSync(fsAddress);
		}

		sendResponse = 1;
	}

	if (requestMessageType == resolveFS && requestMessageMode == req) {
		sendResponse = 1;

		NodeAddress *fsAddress = malloc(sizeof(NodeAddress));

		fsAddress = getFSFromServicePool();

		if (fsAddress == NULL) {
			packetSize = pack(buf, "hh", (int16_t) requestMessageType,
					(int16_t) nack);
			printf(
					"Failed to service resolve request as there is no name server registered currently...\r\n");
		} else {
			char addressString[1024];

			strcpy(addressString, fsAddress->ip);
			strcat(addressString, "-");
			strcat(addressString, fsAddress->portNumber);

			packetSize = pack(buf, "hhs", (int16_t) requestMessageType,
					(int16_t) ack, addressString);

			NodeAddress *clientAddress = malloc(sizeof(NodeAddress));

			clientAddress->ip = malloc(100 * sizeof(char));
			void *ptr = &senderAddress.sin_addr;
			inet_ntop(AF_INET, ptr, clientAddress->ip, 100);
			clientAddress->portNumber = malloc(100 * sizeof(char));
			sprintf(clientAddress->portNumber, "%d", senderAddress.sin_port);

			printf("Serviced file server name resolve request from %s:%s\r\n",
					clientAddress->ip, clientAddress->portNumber);
		}
	}

	if (requestMessageType == syncNS && requestMessageMode == req) {
		sendResponse = 1;
		packetSize = pack(buf, "hh", (int16_t) requestMessageType,
				(int16_t) ack);

		NodeAddress *fsAddress = malloc(sizeof(NodeAddress));
		arg = strtok(arg, NSINFO_LINE_SEP);
		fsAddress->ip = malloc(sizeof(char) * 20);
		strcpy(fsAddress->ip, arg);
		arg = strtok(NULL, NSINFO_LINE_SEP);
		fsAddress->portNumber = malloc(sizeof(char) * 20);
		strcpy(fsAddress->portNumber, arg);

		addFStoPool(fsAddress);

		NodeAddress *otherNSAddress = malloc(sizeof(NodeAddress));

		otherNSAddress->ip = malloc(100 * sizeof(char));
		void *ptr = &senderAddress.sin_addr;
		inet_ntop(AF_INET, ptr, otherNSAddress->ip, 100);
		otherNSAddress->portNumber = malloc(100 * sizeof(char));
		sprintf(otherNSAddress->portNumber, "%d", senderAddress.sin_port);

		printf("Serviced sync request from %s:%s\r\n", otherNSAddress->ip,
				otherNSAddress->portNumber);
	}

	if (requestMessageType == registerNS && requestMessageMode == req) {
		sendResponse = 1;
		packetSize = pack(buf, "hh", (int16_t) requestMessageType,
				(int16_t) ack);

		NodeAddress *otherNSAddress = malloc(sizeof(NodeAddress));
		arg = strtok(arg, NSINFO_LINE_SEP);
		otherNSAddress->ip = malloc(sizeof(char) * 20);
		strcpy(otherNSAddress->ip, arg);
		arg = strtok(NULL, NSINFO_LINE_SEP);
		otherNSAddress->portNumber = malloc(sizeof(char) * 20);
		strcpy(otherNSAddress->portNumber, arg);

		addNSToList(otherNSAddress);

		printf("Serviced register name server request from %s:%s\r\n",
				otherNSAddress->ip, otherNSAddress->portNumber);
	}

	if (sendResponse == 1) {
		if (sendto(controlSocket, buf, packetSize, 0,
				(struct sockaddr *) &senderAddress,
				sizeof(senderAddress)) == SOCKET_ERROR) {
			perror("NS: Error in sending control packet...\r\n");
		}
	}
}

void addFStoPool(NodeAddress *fsAddress) {
	int i = 0;
	int found = 0;

	NodeAddress *temp = malloc(sizeof(NodeAddress));

	for (; i < DArray_count(fsList); i++) {
		temp = DArray_get(fsList, i);

		if (strcmp(temp->portNumber, fsAddress->portNumber) == 0
				&& strcmp(temp->ip, fsAddress->ip) == 0) {
			found = 1;
			break;
		}
	}

	if (found == 0) {
		DArray_push(fsList, fsAddress);
	}
}

NodeAddress *getFSFromServicePool() {
	NodeAddress *fsAddress = NULL;

	if (DArray_count(fsList) > 0) {
		fsAddress = DArray_get(fsList, fsPosition++);

		if (fsPosition == DArray_count(fsList))
			fsPosition = 0;
	}

	return fsAddress;
}

void addNSToList(NodeAddress *nsAddress) {
	int pos = checkNSExists(nsList, nsAddress->ip, nsAddress->portNumber);

	if (pos == -1) {
		DArray_push(nsList, nsAddress);
		nsCount++;
	}
}

void loopOnNeighbouringNSToRegister() {
	int i = 0;
	int success = 0;

	for (; i < nsCount; i++) {
		if (i != nsId) {
			struct NodeAddress *ns = DArray_get(nsList, i);

			printf("Obtained neighbouring name server info - %s:%s...\r\n",
					ns->ip, ns->portNumber);

			success = registerToNS(ns);

			if (success)
				printf(
						"Registration to neighbouring name server is successful...\r\n");
			else
				printf(
						"Registration to neighbouring name server failed...\r\n");
		}
	}
}

int registerToNS(struct NodeAddress *ns) {
	int i = 0;
	int success = 0;

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

	for (; i < numberOfAttempt && success == 0; i++) {
		printf("Requesting Name Server to register... Attempt: %d\r\n", i + 1);

		char addressString[1024];

		strcpy(addressString, localIPv4Address);
		strcat(addressString, "-");
		strcat(addressString, localPort);

		packetSize = pack(buf, "hhs", (int16_t) registerNS, (int16_t) req,
				addressString);

		if (sendto(controlSocket, buf, packetSize, 0,
				(struct sockaddr *) &nsSocketAddr,
				sizeof(nsSocketAddr)) == SOCKET_ERROR) {
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

			if (responseMessageType == registerNS && responseMessageMode == ack)
				success = 1;
		}
	}

	return success;
}

void loopOnNeighbouringNSToSync(NodeAddress *fs) {
	int i = 0;
	int success = 0;

	for (; i < nsCount; i++) {
		if (i != nsId) {

			struct NodeAddress *ns = DArray_get(nsList, i);

			printf("Obtained neighbouring name server info - %s:%s...\r\n",
					ns->ip, ns->portNumber);

			success = syncToNS(ns, fs);

			if (success)
				printf(
						"Synchronisation with neighbouring name server is successful...\r\n");
			else
				printf(
						"Synchronisation with neighbouring name server failed...\r\n");
		}
	}
}

int syncToNS(NodeAddress *ns, NodeAddress *fs) {
	int i = 0;
	int success = 0;

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

	for (; i < numberOfAttempt && success == 0; i++) {
		printf("Requesting Name Server to register... Attempt: %d\r\n", i + 1);

		char addressString[1024];

		strcpy(addressString, fs->ip);
		strcat(addressString, "-");
		strcat(addressString, fs->portNumber);

		packetSize = pack(buf, "hhs", (int16_t) syncNS, (int16_t) req,
				addressString);

		if (sendto(controlSocket, buf, packetSize, 0,
				(struct sockaddr *) &nsSocketAddr,
				sizeof(nsSocketAddr)) == SOCKET_ERROR) {
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

			if (responseMessageType == syncNS && responseMessageMode == ack)
				success = 1;
		}
	}

	return success;
}
