#include "state_machine.h"

int alarmEnabled = FALSE;
int alarmCount = 0;
static int state; 

extern int tries;
extern int timeout;

void alarmHandler(int signal)
{ 
    alarmEnabled = FALSE;
    alarmCount++;

    printf("AlarmCount: %d\n", alarmCount);
    printf("tries: %d\n", tries);
    printf("Alarm #%d\n", alarmCount);
}

// adiciona-se uma variável modo para correr a state machine para information tramas, etc
void setStateMachineTransmitter(int fd, unsigned char *buf_set, unsigned char *buf, u_int8_t a_block, u_int8_t c_block) {
    state = START;
    int bytesW;

    while (alarmCount < tries && state != STOP) {
        if (alarmEnabled == FALSE)
        {
            bytesW = write(fd, buf_set, BUF_SIZE);
            /*
            printf("buf_set = 0x%02X\nbytes written:%d\n", (unsigned int)(buf_set[0] & 0xFF), bytesW);
            printf("buf_set = 0x%02X\nbytes written:%d\n", (unsigned int)(buf_set[1] & 0xFF), bytesW);
            printf("buf_set = 0x%02X\nbytes written:%d\n", (unsigned int)(buf_set[2] & 0xFF), bytesW);
            printf("buf_set = 0x%02X\nbytes written:%d\n", (unsigned int)(buf_set[3] & 0xFF), bytesW);
            printf("buf_set = 0x%02X\nbytes written:%d\n", (unsigned int)(buf_set[4] & 0xFF), bytesW);
            printf("buf_set = 0x%02X\nbytes written:%d\n", (unsigned int)(buf_set[5] & 0xFF), bytesW);
            */

            alarm(timeout); // Set alarm to be triggered in value defined by LinkLayer
            alarmEnabled = TRUE;
        }

        int bytes = read(fd, buf, 1);
        
        //printf("buf_ua = 0x%02X\nbytes written:%d\n", (unsigned int)(buf[0] & 0xFF), bytes);

        if (bytes > 0) {
            switch(state) {
               case START:
                    if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;   
                    }
                    else {
                        state = START;
                    }
                    break; 

                case FLAG_RCV:
                    if (buf[0] == a_block) {
                        state = A_RCV;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;    
                    }
                    else {
                        state = START;
                    }
                    break;

                case A_RCV:
                    if (buf[0] == c_block) {
                        state = C_RCV;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break;

                case C_RCV:
                    if (buf[0] == (a_block ^ c_block)) {
                        state = BCC_OK;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break;

                case BCC_OK:
                    if (buf[0] == FLAG_BLOCK) {
                        state = STOP;
                        printf("Transmitter read response\n");
                        break;
                    }
                    else {
                        state = START;
                    }
                    break;

                case STOP:
                    break;
            }
        }    
    }
}

int sendInfoTrama(int fd, unsigned char *buf, int size) {
    int bytesWritten = 0;

    while (alarmCount < tries)
    {
        if (alarmEnabled == FALSE)
        {
            bytesWritten = write(fd, buf, size);
            printf("Sent I trama\n");
            printf("%d bytes written info\n", bytesWritten);

            alarm(timeout); // Set alarm to be triggered in value defined by LinkLayer
            alarmEnabled = TRUE;
        }
    }
    return bytesWritten;
}

// adiciona-se uma variável modo para correr a state machine para information tramas, etc
void setStateMachineReceiver(int fd, unsigned char *buf, u_int8_t a_block, u_int8_t c_block) {
    state = START;
    
    while (state != STOP) {
        int bytes = read(fd, buf, 1);

        if (bytes > 0) {
            switch(state) {
                case START:
                    if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break; 

                case FLAG_RCV:
                    if (buf[0] == A_BLOCK_SET) {
                        state = A_RCV;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;    
                    }
                    else {
                        state = START;
                    }
                    break;

                case A_RCV:
                    if (buf[0] == C_BLOCK_SET) {
                        state = C_RCV;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break;

                case C_RCV:
                    if (buf[0] == (A_BLOCK_SET ^ C_BLOCK_SET)) {
                        state = BCC_OK;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break;

                case BCC_OK:
                    if (buf[0] == FLAG_BLOCK) {
                        state = STOP;
                        printf("Receiver read SET message\n");
                        break;
                    }
                    else {
                        state = START;
                    }
                    break;

                case STOP:
                    break;
            }
        }    
    }

    buf[0] = FLAG_BLOCK;
    buf[1] = a_block;
    buf[2] = c_block;
    buf[3] = (a_block ^ c_block);
    buf[4] = FLAG_BLOCK;
    buf[5] = '\n';

    int bytes = write(fd, buf, BUF_SIZE);
    printf("bytes writen for the ua: %d\n", bytes);

    if (bytes < 0) return;
}


void setStateMachineReceiverInf(int fd, unsigned char* buf, unsigned char* buf_information, u_int8_t a_block) {
    int i = 0;
    state = START;

    size_t size = sizeof(buf_information);
    printf("Size of buf_information: %zu\n", size);

    while (state != STOP) {
        int bytes = read(fd, buf, 1);
        if (bytes > 0) printf("buf = 0x%02X\n%d\n", (unsigned int)(buf[0] & 0xFF), bytes);

        if (bytes > 0) {
            switch(state) {
                case START:
                    printf("Hello\n");
                    if (buf[0] == FLAG_BLOCK) {
                        buf_information[i++] = buf[0];
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break; 

                case FLAG_RCV:
                    printf("FLAG\n");
                    printf("buf = 0x%02X,%d\n", (unsigned int)(buf[0] & 0xFF), bytes);
                    if (buf[0] == a_block) {
                        buf_information[i++] = buf[0];
                        state = A_RCV;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;    
                    }
                    else {
                        i = START;
                        state = START;
                    }
                    break;

                case A_RCV:
                    printf("A\n");
                    printf("buf = 0x%02X,%d\n", (unsigned int)(buf[0] & 0xFF), bytes);
                    if (buf[0] == 0x00 || buf[0] == 0x40) {
                        buf_information[i++] = buf[0];
                        state = C_RCV;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        i = FLAG_RCV;
                        state = FLAG_RCV;
                    }
                    else {
                        i = START;
                        state = START;
                    }
                    break;

                case C_RCV:
                    printf("C\n");
                    printf("buf = 0x%02X,%d\n", (unsigned int)(buf[0] & 0xFF), bytes);
                    if (buf[0] == (a_block ^ 0x00) || buf[0] == (a_block ^ 0x40)) {
                        buf_information[i++] = buf[0];
                        state = BCC_OK;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        i = FLAG_RCV;
                        state = FLAG_RCV;
                    }
                    else {
                        i = START;
                        state = START;
                    }
                    break;

                case BCC_OK:
                    printf("BCC_OK\n");
                    printf("buf = 0x%02X,%d\n", (unsigned int)(buf[0] & 0xFF), bytes);
                    if (buf[0] == FLAG_BLOCK) {
                        buf_information[i] = buf[0];
                        state = STOP;
                        printf("DONE!!!\n");
                        break;
                    }
                    else {
                        buf_information[i++] = buf[0];
                    }
                    break;

                case STOP:
                    break;
            }
        }    
    }
}

int setStateMachineReceiverSup(int fd, unsigned char* buf, u_int8_t a_block, unsigned char* wantedBytes) {
    printf("Got inside setStateMachineReceiverSup\n");
    state = START;
    u_int8_t c_block;
    int resend; // 0 = RR, 1 == REJ
    while (state != STOP) {
        // printf("Got inside setStateMachineReceiverSup while loop\n");
        int bytes = read(fd, buf, 1);
        //printf("buf = 0x%02X\n%d\n", (unsigned int)(buf[0] & 0xFF), bytes);

        if (bytes > 0) {
            switch(state) {
                case START:
                    printf("Hello\n");
                    if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break; 

                case FLAG_RCV:
                    printf("FLAG\n");
                    printf("buf = 0x%02X,%d\n", (unsigned int)(buf[0] & 0xFF), bytes);
                    if (buf[0] ==  a_block) {
                        state = A_RCV;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;    
                    }
                    else {
                        state = START;
                    }
                    break;

                case A_RCV:
                    printf("A\n");
                    printf("buf = 0x%02X,%d\n", (unsigned int)(buf[0] & 0xFF), bytes);
                    if (buf[0] == 0x05 || buf[0] == 0x85) {
                        printf("GOT INSIDE RR\n");
                        resend = 0;
                        c_block = buf[0];
                        state = C_RCV;
                    }
                    else if (buf[0] == 0x01 || buf[0] == 0x81) {
                        printf("GOT INSIDE REJ\n");
                        resend = 1;
                        c_block = buf[0];
                        state = C_RCV;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break;

                case C_RCV:
                    printf("C\n");
                    printf("buf = 0x%02X,%d\n", (unsigned int)(buf[0] & 0xFF), bytes);
                    if (buf[0] == (a_block ^ c_block)) {
                        state = BCC_OK;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break;

                case BCC_OK:
                    printf("BCC_OK\n");
                    printf("buf = 0x%02X,%d\n", (unsigned int)(buf[0] & 0xFF), bytes);
                    if (buf[0] == FLAG_BLOCK) {
                        state = STOP;
                        printf("DONE!!!\n");
                        break;
                    }
                    else {
                        state = START;
                    }
                    break;

                case STOP:
                    break;
            }
        }    
    }
    return resend;
}

void setStateMachineReceiverDisc(int fd, unsigned char *buf, u_int8_t a_block, u_int8_t c_block) {
    state = START;
    while (state != STOP) {
        int bytes = read(fd, buf, 1);

        if (bytes > 0) {
            switch(state) {
                case START:
                    printf("Hello\n");
                    if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break; 

                case FLAG_RCV:
                    printf("FLAG\n");
                    printf("buf = 0x%02X,%d\n", (unsigned int)(buf[0] & 0xFF), bytes);
                    if (buf[0] == a_block) {
                        state = A_RCV;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;    
                    }
                    else {
                        state = START;
                    }
                    break;

                case A_RCV:
                    printf("A\n");
                    printf("buf = 0x%02X,%d\n", (unsigned int)(buf[0] & 0xFF), bytes);
                    if (buf[0] == c_block) {
                        state = C_RCV;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break;

                case C_RCV:
                    printf("C\n");
                    printf("buf = 0x%02X,%d\n", (unsigned int)(buf[0] & 0xFF), bytes);
                    if (buf[0] == (a_block ^ c_block)) {
                        state = BCC_OK;
                    }
                    else if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break;

                case BCC_OK:
                    printf("BCC_OK\n");
                    printf("buf = 0x%02X,%d\n", (unsigned int)(buf[0] & 0xFF), bytes);
                    if (buf[0] == FLAG_BLOCK) {
                        state = STOP;
                        printf("DONE!!!\n");
                        break;
                    }
                    else {
                        state = START;
                    }
                    break;

                case STOP:
                    break;
            }
        }    
    }
} 