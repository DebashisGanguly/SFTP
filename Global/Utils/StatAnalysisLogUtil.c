/*
 * StatAnalysisLogUtil.c
 *
 *  Created on: 4 Apr 2015
 *      Author: Debashis Ganguly
 */

#include "../SFTP.h"

void writeStatAnalysisLog(char *fileName, long fileSize, double rtt) {
	FILE *fp = fopen(statAnalysisLogPath, "a+");

	fprintf(fp, "%s\t%ld\t%lf\t%d\t%lf\t%lf\t%d\t%d\r\n", fileName, fileSize, rtt, windowSize, bitErrorProbabiliy, congestionPercentage, timeOutusec, mtu);

	fclose(fp);
}

double getRandomValueForAnalysis() {
	return (double) 1.0 * rand() / (RAND_MAX + 1.0);
}
