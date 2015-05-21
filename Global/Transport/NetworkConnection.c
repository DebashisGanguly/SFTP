/*
 * ControlConnection.c
 *
 *  Created on: 11-Mar-2015
 *      Author: Debashis Ganguly
 */

#include "../SFTP.h"

int createNetworkChannelReceiver(char *localPort) {
	struct sockaddr_in localAddress;
	int on = 1;
	int err;
	int localSocket;

	if ((localSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		perror("Control socket creation failed...");
		return -1;
	}

	err = setsockopt(localSocket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	if (err < 0) {
		perror("Setting data sock option failed...");
		close(localSocket);
		return -1;
	}

	memset(&localAddress, 0, sizeof(localAddress));

	localAddress.sin_family = AF_INET;
	localAddress.sin_port = 0;
	localAddress.sin_addr.s_addr = htonl(INADDR_ANY);

	err = bind(localSocket, (struct sockaddr*) &localAddress,
			sizeof(localAddress));

	if (err < 0) {
		perror("Binding data socket failed...\r\n");
		close(localSocket);
		return -1;
	}

	int len = sizeof(localAddress);
	getsockname(localSocket, (struct sockaddr*) &localAddress, &len);

	sprintf(localPort, "%d", localAddress.sin_port);

	printf("Control channel created...\r\n");

	return localSocket;
}

int createNetworkChannelSender() {
	int remoteSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (remoteSocket < 0) {
		perror("Sender control socket creation failed...");
		return SOCKET_ERROR;
	}

	return remoteSocket;
}
