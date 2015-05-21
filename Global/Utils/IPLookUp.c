/*
 * IPLookUp.c
 *
 *  Created on: 11-Mar-2015
 *      Author: Debashis Ganguly
 */

#include "../SFTP.h"

char* getIPv4Address() {
	struct addrinfo hints, *res;
	int errcode;
	char *addrstr = malloc(100 * sizeof(char));
	void *ptr;

	char hostname[1024];

	hostname[1023] = '\0';
	gethostname(hostname, 1023);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;

	errcode = getaddrinfo(hostname, NULL, &hints, &res);

	if (errcode != 0) {
		perror("Error getting local address information...");
		return NULL;
	}

	while (res) {
		if((res->ai_family == PF_INET6 ? 6 : 4) == 4) {
			ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
			inet_ntop(res->ai_family, ptr, addrstr, 100);

			break;
		}

		res = res->ai_next;
	}

	return addrstr;
}
