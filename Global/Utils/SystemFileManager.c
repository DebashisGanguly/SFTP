/*
 * SystemFileManager.c
 *
 *  Created on: 09-Mar-2015
 *      Author: Debashis Ganguly
 *     Purpose: This file has functions to read config file to get name_server_info.txt
 *     			and read and write there
 */

#include "../SFTP.h"

int checkNull(char*);
int checkInteger(char*);
int checkMinimum(char*, int);
int checkRange(char*, int, int);
int checkDouble(char*);
int checkPath(char*);

void validateAndPopulateConfigurationItems() {
	char* line = NULL;
	char* key = malloc(sizeof(char) * MAX_LINE);
	char* value = malloc(sizeof(char) * MAX_LINE);

	size_t len = 0;
	ssize_t read;

	int status = 0, configItems = 0;

	FILE *fp;

	if ((fp = fopen(ENV_CONFIG_FILE, "r")) == NULL) {
		perror("Error to open the configuration file...");
	}

	while ((read = getline(&line, &len, fp)) != -1) {
		if ((line = strtok(line, CONF_LINE_SEP)) == NULL || *line == 0)
			continue;
		strcpy(key, line);

		if ((line = strtok(NULL, CONF_LINE_SEP)) == NULL || *line == 0)
			continue;
		strcpy(value, line);

		if (strcmp(key, TIME_OUT) == 0) {
			status = checkNull(value);
			if (!status) {
				printf(
						"Configuration Error: please define time out in the configuration file...\r\n");
				continue;
			}
			status = checkInteger(value);
			if (!status) {
				printf(
						"Configuration Error: please provide an integer value for time out (microseconds) in the configuration file...\r\n");
				continue;
			}
			status = checkMinimum(value, 0);
			if (!status) {
				printf(
						"Configuration Error: please define time out (microseconds) with positive value...\r\n ");
				continue;
			}
			timeOutusec = atoi(value);
			configItems++;
		} else if (strcmp(key, ATTEMPT_NO) == 0) {
			status = checkNull(value);
			if (!status) {
				printf(
						"Configuration Error: please define number of attempts in the configuration file...\r\n");
				continue;
			}
			status = checkInteger(value);
			if (!status) {
				printf(
						"Configuration Error: please provide an integer value for number of attempts in the configuration file...\r\n ");
				continue;
			}
			status = checkMinimum(value, 0);
			if (!status) {
				printf(
						"Configuration Error: please define number of attempts with positive value...\r\n");
				continue;
			}
			numberOfAttempt = atoi(value);
			configItems++;
		} else if (strcmp(key, NAME_SERVER_INFO_CONFIG_ITEM) == 0) {
			status = checkNull(strtok(value, "\r\n"));
			if (!status) {
				printf(
						"Configuration Error: please define Name_Server_Info file path in the configuration file...\r\n");
				continue;
			}
			status = checkPath(strtok(value, "\r\n"));
			if (status == -1) {
				printf(
						"Configuration Error: Name_Server_Infofile not found at the given path...\r\n");
				continue;
			} else if (status == -2) {
				printf(
						"Configuration Error: Name_Server_Info file is not readable...\r\n");
				continue;
			} else if (status == -3) {
				printf(
						"Configuration Error: Name_Server_Info file is not writable...\r\n");
				continue;
			}
			strcpy(nameServerInfoPath, strtok(value, "\r\n"));
			configItems++;
		} else if (strcmp(key, SFTP_FS) == 0) {
			status = checkNull(value);
			if (!status) {
				printf(
						"Configuration Error: please define File server logical name in the configuration file...\r\n");
				continue;
			}
			strcpy(sftpLogicalName, strtok(value, "\r\n"));
			configItems++;
		} else if (strcmp(key, MTU) == 0) {
			status = checkNull(value);
			if (!status) {
				printf(
						"Configuration Error: please define MTU in the configuration file...\r\n");
				continue;
			}
			status = checkInteger(value);
			if (!status) {
				printf(
						"Configuration Error: please provide an integer value for MTU in the configuration file...\r\n");
				continue;
			}
			status = checkRange(value, 0, 1500);
			if (!status) {
				printf(
						"Configuration Error: please define MTU with positive value between 0 and 1500 bytes...\r\n");
				continue;
			}
			mtu = atoi(value);
			configItems++;
		} else if (strcmp(key, WINDOW_SIZE) == 0) {
			status = checkNull(value);
			if (!status) {
				printf(
						"Configuration Error: please define Window Size in the configuration file...\r\n");
				continue;
			}
			status = checkInteger(value);
			if (!status) {
				printf(
						"Configuration Error: please provide an integer value for Window Size in the configuration file...\r\n");
				continue;
			}
			status = checkMinimum(value, 0);
			if (!status) {
				printf(
						"Configuration Error: please define a positive Window Size...\r\n");
				continue;
			}
			windowSize = atoi(value);
			configItems++;
		} else if (strcmp(key, INJ_ERROR_TEST) == 0) {
			status = checkNull(value);
			if (!status) {
				printf(
						"Configuration Error: please define Error Injection flag in the configuration file...\r\n");
				continue;
			}
			status = checkInteger(value);
			if (!status) {
				printf(
						"Configuration Error: please provide an integer value for Error Injection flag in the configuration file...\r\n");
				continue;
			}
			status = checkRange(value, 0, 1);
			if (!status) {
				printf(
						"Configuration Error: please define Error Injection flag either 0 or 1...\r\n");
				continue;
			}
			injErrorTest = atoi(value);
			configItems++;
		} else if (strcmp(key, P_ERR) == 0) {
			status = checkNull(value);
			if (!status) {
				printf(
						"Configuration Error: please define probability of a bit in error in the configuration file...\r\n");
				continue;
			}
			status = checkDouble(value);
			if (!status) {
				printf(
						"Configuration Error: please provide a decimal value for probability of a bit in error in the configuration file...\r\n");
				continue;
			}
			status = checkRange(value, 0, 1);
			if (!status) {
				printf(
						"Configuration Error: please define probability of a bit in error between 0 and 1...\r\n");
				continue;
			}
			bitErrorProbabiliy = atof(value);
			configItems++;
		} else if (strcmp(key, P_CONG) == 0) {
			status = checkNull(value);
			if (!status) {
				printf(
						"Configuration Error: please define Congestion percentage in the configuration file...\r\n");
				continue;
			}
			status = checkDouble(value);
			if (!status) {
				printf(
						"Configuration Error: please provide a decimal value for Congestion percentage in the configuration file...\r\n");
				continue;
			}
			status = checkRange(value, 0, 100);
			if (!status) {
				printf(
						"Configuration Error: please define Congestion percentage between 0 and 100...\r\n");
				continue;
			}
			congestionPercentage = atof(value);
			configItems++;
		} else if (strcmp(key, STAT_ANALYSIS_LOG_FILE) == 0) {
			status = checkNull(strtok(value, "\r\n"));
			if (!status) {
				printf(
						"Configuration Error: please define Stat_Analysis_Log_File file path in the configuration file...\r\n");
				continue;
			}
			status = checkPath(strtok(value, "\r\n"));
			if (status == -1) {
				printf(
						"Configuration Error: Stat_Analysis_Log_File not found at the given path...\r\n");
				continue;
			} else if (status == -2) {
				printf(
						"Configuration Error: Stat_Analysis_Log_File file is not readable...\r\n");
				continue;
			} else if (status == -3) {
				printf(
						"Configuration Error: Stat_Analysis_Log_File file is not writable...\r\n");
				continue;
			}
			strcpy(statAnalysisLogPath, strtok(value, "\r\n"));
			configItems++;
		}
	}

	//if (fp)
	//fclose(fp);

	if (configItems != TOTAL_CONFIG_ITEMS) {
		printf(
				"Please correct the configuration file and restart the application...\r\n");
		exit(0);
	}
}

int checkNull(char* value) {
	value = strtok(value, "\r\n");
	if (value == NULL)
		return 0;
	return 1;
}

int checkInteger(char* value) {
	char* str = malloc(sizeof(char) * MAX_LINE);
	strtol(value, &str, 10);
	if (strcmp(value, str) == 0)
		return 0;
	return 1;
}

int checkMinimum(char* value, int min) {
	int numberValue = atoi(value);
	if (numberValue < min)
		return 0;
	return 1;
}

int checkRange(char* value, int min, int max) {
	int numberValue = atoi(value);
	if (numberValue < min && numberValue > max)
		return 0;
	return 1;
}

int checkDouble(char* value) {
	char* str = malloc(sizeof(char) * MAX_LINE);
	strtod(value, &str);
	if (strcmp(value, str) == 0)
		return 0;
	return 1;
}

int checkPath(char* value) {
	if (access(value, F_OK) != 0) {
		return -1;
	} else if (access(value, R_OK) != 0) {
		return -2;
	} else if (access(value, W_OK) != 0) {
		return -3;
	}
	return 1;
}
