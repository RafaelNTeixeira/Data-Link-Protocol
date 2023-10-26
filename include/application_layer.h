#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_

#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include "link_layer.h"

#define C_DATA       0x01
#define C_START      0x02
#define C_END        0x03

#define MAX_SIZE     1024  	

unsigned char* dataPackConstructor(unsigned char* message, int bufSize);
int controlPackConstructor(unsigned char startEnd, unsigned char *packet, int fileSize, char *fileName);

int dataPackReader(unsigned char *packet, unsigned char *message);
int controlPackReader(unsigned char *packet, int fileSize, char *fileName);

int sendFile(char serialPort[50], char *fileName, int baudRate, int nRetransmissions, int timeout);
int readFile(char serialPort[50], char *fileName, int baudRate, int nRetransmissions, int timeout);
 
/* AUX */
long int getFileSize(FILE *file);

#endif // _APPLICATION_LAYER_H_
