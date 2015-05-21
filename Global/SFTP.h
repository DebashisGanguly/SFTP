/*
 * SFTP.h
 *
 *  Created on: 09-Mar-2015
 *      Author: Debashis Ganguly
 */

#ifndef GLOBAL_SFTP_H_
#define GLOBAL_SFTP_H_

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>

#include "../Global/DArray.h"

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#ifndef MAX_BUF_LEN
#define MAX_BUF_LEN 1500
#endif

#define ENV_CONFIG_FILE "/afs/cs.pitt.edu/usr0/darshan/private/SFTP/Config/sftp.conf"
#define CONF_LINE_SEP "="
#define NSINFO_LINE_SEP "-"
#define ARG_SEP ","
#define MAX_LINE  4096

#define NAME_SERVER_INFO_CONFIG_ITEM "Name_Server_Info"

#define TOTAL_CONFIG_ITEMS 10

#define STAT_ANALYSIS_LOG_FILE "Stat_Analysis_Log_File"
#define ATTEMPT_NO "Attempt_No"
#define TIME_OUT "Time_Out"
#define WINDOW_SIZE "Window_Size"
#define MTU "MTU"
#define SFTP_FS "SFTP_FS"
#define INJ_ERROR_TEST "Inj_Error_Test"
#define P_ERR "P_Err"
#define P_CONG "P_Cong"

int timeOutusec;
int numberOfAttempt;
int mtu;
int windowSize;
int injErrorTest;
double bitErrorProbabiliy;
double congestionPercentage;

char nameServerInfoPath[MAX_LINE];
char sftpLogicalName[MAX_LINE];
char statAnalysisLogPath[MAX_LINE];

typedef enum messageType {
	registerFS,
	resolveFS,
	registerNS,
	syncNS,
	sftpCmd,
	connectFS,
	disconnectFS,
	noneCtrlId,
	estDataConn,
	terDataConn,
	sendFile,
	startTransfer
} messageType;

typedef enum messageMode {
	req, ack, nack
} messageMode;

typedef enum sftpCtrlCommand {
	rls, rcd, rpwd, get, put, mget, mput
} sftpCtrlCommand;

typedef enum dataPacketIdentifier {
	dataPacket, finMessage
} dataPacketIdentifier;

typedef enum finMessageType {
	fin, finAck, finalAck
} finMessageType;

typedef struct NodeAddress {
	char *portNumber;
	char *ip;
} NodeAddress;

u_int16_t crc16(unsigned short length, int8_t data_p[length]);

int32_t pack(unsigned char *buf, char *format, ...);
int32_t unpack(unsigned char *buf, char *format, ...);

int writeNSInfo(char *ip, char *port, int nsId);
DArray *readNSInfo();
int checkNSExists(DArray *nsList, char *ip, char *port);
int getRandomNSPosition(int countNS);

char* getIPv4Address();

void validateAndPopulateConfigurationItems();

DArray *getAllFilesInCurrentDirectory();
char* getCurrentWorkingDirectory();
int changeDirectory(char *path);
int getDirectoryListing(char directoryListBuf[], int directoryListBufLen);

int createNetworkChannelReceiver(char *localPort);
int createNetworkChannelSender();

int canWriteFile(char *fileName);
DArray *getMatchingFilesInCurrentDirectory(char *arg);
int establishGetDataConnectionToFS(int localControlSocket, char *localIPv4Address,
		char *localDataPort, NodeAddress *fsPrivateAddress,
		sftpCtrlCommand cmdId, char *arg);
int establishPutDataConnectionToFS(int localControlSocket,
		NodeAddress *fsPrivateAddress, sftpCtrlCommand cmdId,
		struct sockaddr_in *serverDataSocketAddress);

int sendFileTransferRequestPut(int controlSocket, NodeAddress *remoteAddress,
		char *fileName);
int sendFileTransferRequestGet(int controlSocket, NodeAddress *remoteAddress,
		char *fileName);
int startFileTransfer(int controlSocket, NodeAddress *remoteAddress);
int disconnectDataConnection(int localControlSocket, NodeAddress *remoteAddress);

typedef struct DataPacket {
	int16_t sequenceNumber;
	int16_t fragmentationFlag;
	int16_t dataLength;
	u_int16_t crc;
	int8_t *data;
} DataPacket;

void dataFileSender(int dataSocket, struct sockaddr_in remoteAddress,
		char *fileName);
void dataFileReceiver(int dataSocket, char fileName[MAX_LINE]);

double getRandomValueForAnalysis();
void writeStatAnalysisLog(char *fileName, long fileSize, double rtt);

#endif /* GLOBAL_SFTP_H_ */
