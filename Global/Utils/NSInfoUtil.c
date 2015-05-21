/*
 * NSInfoUtil.c
 *
 *  Created on: 11-Mar-2015
 *      Author: Debashis Ganguly
 */

#include "../SFTP.h"
#include "../DArray.h"

int getRandomNSPosition(int countNS) {
	srand(time(NULL));

	return rand() % countNS;
}

int writeNSInfo(char *ip, char *port, int nsId) {
	FILE *fp;

	if ((fp = fopen(nameServerInfoPath, "a+")) == NULL) {
		perror("Error to open the configuration file...");
		return -1;
	}

	fprintf(fp, "%d-%s-%s\r\n", nsId, ip, port);

	printf("Updated Name Server Info with %d-%s-%s\r\n", nsId, ip, port);

	fclose(fp);
	return 0;
}

DArray *readNSInfo() {
	DArray *nsList = DArray_create(sizeof(NodeAddress), 10);

	FILE *fp;

	char* line = NULL;

	size_t len = 0;
	ssize_t read;

	if ((fp = fopen(nameServerInfoPath, "r")) == NULL) {
		perror("Error to open the configuration file...");
		return NULL;
	}

	char* ip = malloc(sizeof(char) * 20);
	int port;

	while ((read = getline(&line, &len, fp)) != -1) {
		if ((line = strtok(line, NSINFO_LINE_SEP)) == NULL || *line == 0)
			continue;

		if ((line = strtok(NULL, NSINFO_LINE_SEP)) == NULL || *line == 0)
			continue;

		strcpy(ip, line);

		if ((line = strtok(NULL, NSINFO_LINE_SEP)) == NULL || *line == 0)
			continue;

		port = atoi(line);

		char portNumber[20];

		sprintf(portNumber, "%d", port);

		struct NodeAddress *address = DArray_new(nsList);
		address->portNumber = malloc(sizeof(char) * 20);
		strcpy(address->portNumber, portNumber);
		address->ip = malloc(sizeof(char) * 20);
		strcpy(address->ip, ip);

		DArray_push(nsList, address);
	}

	fclose(fp);

	return nsList;
}

int checkNSExists(DArray *nsList, char *ip, char *port) {
	int pos = -1;
	int i = 0;
	int numberOfNS = DArray_count(nsList);

	for (; i < numberOfNS; i++) {
		struct NodeAddress *ns = DArray_get(nsList, i);

		if (strcmp(ns->ip, ip) == 0 && strcmp(ns->portNumber, port) == 0) {
			pos = i;
		}
	}

	return pos;
}
