#include "state_machine.h"

int alarmEnabled = FALSE;
static int state; 
int alarmCount = 0;

extern int tries;
extern int timeout;
extern struct termios oldtio;

void alarmHandler(int signal)
{ 
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

int setStateMachineTransmitter(int fd, unsigned char *buf_set, unsigned char *buf, u_int8_t a_block, u_int8_t c_block) {
    state = START;

    while (alarmCount < tries && state != STOP) {
        if (alarmEnabled == FALSE)
        {
            write(fd, buf_set, BUF_SIZE);

            alarm(timeout); // Set alarm to be triggered in value defined by LinkLayer
            alarmEnabled = TRUE;
        }

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
                        alarmEnabled = FALSE;
                        alarm(0);
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
    return alarmCount;
}

int setStateMachineWrite(int fd, unsigned char *buf, int size, int* bytesWritten, unsigned char* frameNumberByte) {
    alarmCount = 0;
    *bytesWritten = 0;
    int resend;
    state = START;

    while (alarmCount < tries && state != STOP)
    {
        if (alarmEnabled == FALSE)
        {
            *bytesWritten = write(fd, buf, size);
            printf("Sent I trama\n");
            printf("%d bytes written info\n", *bytesWritten);

            alarm(timeout); // Set alarm to be triggered in value defined by LinkLayer
            alarmEnabled = TRUE;
        }

        if (*bytesWritten < 0) {
            sleep(1);

            if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
                perror("tcsetattr");
                return -1;
            }
        }

        unsigned char wantedBytes[2];
        
        if (*frameNumberByte == 0x00) {
            wantedBytes[0] = 0x85;
            wantedBytes[1] = 0x01;
        }
        else if (*frameNumberByte == 0x40) {
            wantedBytes[0] = 0x05;
            wantedBytes[1] = 0x81;
        }

        unsigned char buf_super[BUF_SIZE] = {0};
        u_int8_t c_block;

        int bytes = read(fd, buf_super, 1);

        if (bytes > 0) {
            switch(state) {
                case START:
                    if (buf_super[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break; 

                case FLAG_RCV:
                    if (buf_super[0] ==  0x03) {
                        state = A_RCV;
                    }
                    else if (buf_super[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;    
                    }
                    else {
                        state = START;
                    }
                    break;

                case A_RCV:
                    if (buf_super[0] == wantedBytes[0]) {
                        resend = 0;
                        printf("GOT INSIDE RR. RESPONSE: %d\n", wantedBytes[0]);
                        c_block = buf_super[0];
                        state = C_RCV;
                    }
                    else if (buf_super[0] == wantedBytes[1]) {
                        resend = 1;
                        printf("GOT INSIDE REJ. RESPONSE: %d\n", wantedBytes[1]);
                        c_block = buf_super[0];
                        state = C_RCV;
                    }
                    else if (buf_super[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break;

                case C_RCV:
                    if (buf_super[0] == (0x03 ^ c_block)) {
                        state = BCC_OK;
                    }
                    else if (buf_super[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break;

                case BCC_OK:
                    if (buf_super[0] == FLAG_BLOCK) {
                        state = STOP;
                        alarmEnabled = FALSE;
                        alarm(0);
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
        if (alarmCount >= tries) return -1;
    }
    return resend;
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



int setStateMachineReceiverInf(int fd, unsigned char* buf, unsigned char* buf_information, u_int8_t a_block) {
    int i = 0;
    int data_size = 0;
    state = START;

    while (state != STOP) {
        int bytes = read(fd, buf, 1);

        if (bytes > 0) {
            switch(state) {
                case START:
                    if (buf[0] == FLAG_BLOCK) {
                        buf_information[i++] = buf[0];
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break; 

                case FLAG_RCV:
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
                    if (buf[0] == FLAG_BLOCK) {
                        buf_information[i] = buf[0];
                        state = STOP;
                        break;
                    }
                    else {
                        buf_information[i++] = buf[0];
                        data_size++;
                    }
                    break;

                case STOP:
                    break;
            }
        }    
    }
    return data_size;
}

int setStateMachineReceiverSup(int fd, unsigned char* buf, u_int8_t a_block, unsigned char* wantedBytes) {
    state = START;
    u_int8_t c_block;
    int resend; // 0 = RR, 1 = REJ
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
                    if (buf[0] == 0x05 || buf[0] == 0x85) {
                        resend = 0;
                        c_block = buf[0];
                        state = C_RCV;
                    }
                    else if (buf[0] == 0x01 || buf[0] == 0x81) {
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

void sendUA(int fd, unsigned char *buf_ua, int bufSize) {
     while (alarmCount < tries) {
        if (alarmEnabled == FALSE)
        {
            printf("Sent UA trama\n");
            write(fd, buf_ua, bufSize);

            alarm(timeout); // Set alarm to be triggered in value defined by LinkLayer
            alarmEnabled = TRUE;
        }
     }
}
