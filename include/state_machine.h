#ifndef _STATE_MACHINE_H_
#define _STATE_MACHINE_H_

#include "link_layer.h"

#define START 0
#define FLAG_RCV 1
#define A_RCV 2
#define C_RCV 3
#define BCC_OK 4
#define STOP 5

#define FLAG_BLOCK 0x7E

#define A_BLOCK_SET    0x03
#define C_BLOCK_SET    0x03
#define BCC_BLOCK_SET  0x01 ^ 0x03

#define A_BLOCK_UA    0x01
#define C_BLOCK_UA    0x07
#define BCC_BLOCK_UA  0x03 ^ 0x07

#define A_BLOCK_DISC_TRANS  0x03
#define A_BLOCK_DISC_REC    0x01
#define C_BLOCK_DISC        0x0B

#define A_BLOCK_INF_TRANS  0x03
#define A_BLOCK_INF_REC    0x01
#define C_BLOCK_INF_TRANS  0x00
#define C_BLOCK_INF_REC    0x40

#define END 0x03

void alarmHandler(int signal);
void setStateMachineTransmitter(int fd, unsigned char *buf_set, unsigned char *buf, u_int8_t a_block, u_int8_t c_block);
void setStateMachineReceiver(int fd, unsigned char *buf, u_int8_t a_block, u_int8_t c_block);
int setStateMachineWrite(int fd, unsigned char *buf, int size, int* bytesWritten, unsigned char* frameNumberByte);
int setStateMachineReceiverInf(int fd, unsigned char *buf, unsigned char* buf_information, u_int8_t a_block);
int setStateMachineReceiverSup(int fd, unsigned char *buf, u_int8_t a_block, unsigned char* wantedBytes);
void setStateMachineReceiverDisc(int fd, unsigned char *buf, u_int8_t a_block, u_int8_t c_block);
void sendUA(int fd, unsigned char *buf_ua, int bufSize);

#endif