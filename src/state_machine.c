#include "state_machine.h"

int alarmEnabled = FALSE;
int alarmCount = 0;
static int state; 

extern int tries;
extern int timeout;
extern struct termios oldtio;

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
            //write(fd, buf_set, BUF_SIZE);
            
            printf("buf_set = 0x%02X\nbytes written:%d\n", (unsigned int)(buf_set[0] & 0xFF), bytesW);
            printf("buf_set = 0x%02X\nbytes written:%d\n", (unsigned int)(buf_set[1] & 0xFF), bytesW);
            printf("buf_set = 0x%02X\nbytes written:%d\n", (unsigned int)(buf_set[2] & 0xFF), bytesW);
            printf("buf_set = 0x%02X\nbytes written:%d\n", (unsigned int)(buf_set[3] & 0xFF), bytesW);
            printf("buf_set = 0x%02X\nbytes written:%d\n", (unsigned int)(buf_set[4] & 0xFF), bytesW);

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

int setStateMachineWrite(int fd, unsigned char *buf, int size, int* bytesWritten, unsigned char* frameNumberByte) {
    bytesWritten = 0;
    int resend;
    state = START;

    while (alarmCount < tries && state != STOP)
    {
        if (alarmEnabled == FALSE)
        {
            bytesWritten = write(fd, buf, size);
            printf("Sent I trama\n");
            printf("%d bytes written info\n", bytesWritten);

            alarm(timeout); // Set alarm to be triggered in value defined by LinkLayer
            alarmEnabled = TRUE;
        }

        if (bytesWritten < 0) {
            sleep(1);

            if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
                perror("tcsetattr");
                return -1;
            }
        }

        if (buf[4] == 0x03) return 2;

        unsigned char wantedBytes[2];
        
        if (frameNumberByte == 0x00) {
            wantedBytes[0] = 0x85;
            wantedBytes[1] = 0x01;
        }
        else if (frameNumberByte == 0x40) {
            wantedBytes[0] = 0x05;
            wantedBytes[1] = 0x81;
        }
        //printf("GOT HERE2\n");

        unsigned char buf_super[BUF_SIZE] = {0};
        u_int8_t c_block;

        int bytes = read(fd, buf_super, 1);
        // if (bytes > 0) printf("buf_super = 0x%02X\n%d\n", (unsigned int)(buf_super[0] & 0xFF), bytes);
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
                    printf("FLAG\n");
                    printf("buf_super = 0x%02X,%d\n", (unsigned int)(buf_super[0] & 0xFF), bytes);
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
                    printf("A\n");
                    printf("buf_super = 0x%02X,%d\n", (unsigned int)(buf_super[0] & 0xFF), bytes);
                    if (buf_super[0] == wantedBytes[0]) {
                        resend = 0;
                        printf("GOT INSIDE RR. Resend: %d\n", resend);
                        c_block = buf_super[0];
                        state = C_RCV;
                    }
                    else if (buf_super[0] == wantedBytes[1]) {
                        printf("GOT INSIDE REJ. Resend: %d\n", resend);
                        resend = 1;
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
                    printf("C\n");
                    printf("buf_super = 0x%02X,%d\n", (unsigned int)(buf_super[0] & 0xFF), bytes);
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
                    printf("BCC_OK\n");
                    printf("buf_super = 0x%02X,%d\n", (unsigned int)(buf_super[0] & 0xFF), bytes);
                    if (buf_super[0] == FLAG_BLOCK) {
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
    state = START;

    while (state != STOP) {
        int bytes = read(fd, buf, 1);
        if (bytes > 0) printf("buf = 0x%02X\n%d\n", (unsigned int)(buf[0] & 0xFF), bytes);

        if (bytes > 0) {
            switch(state) {
                case START:
                    if (buf[0] == FLAG_BLOCK) {
                        buf_information[i++] = buf[0];
                        //printf("buf_information = 0x%02X\n", (unsigned int)(buf_information[i-1] & 0xFF));
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
                        //printf("buf_information = 0x%02X\n", (unsigned int)(buf_information[i-1] & 0xFF));
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
                        //printf("buf_information = 0x%02X\n", (unsigned int)(buf_information[i-1] & 0xFF));
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
                        //printf("buf_information = 0x%02X\n", (unsigned int)(buf_information[i-1] & 0xFF));
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
                        //printf("buf_information = 0x%02X\n", (unsigned int)(buf_information[i] & 0xFF));
                        state = STOP;
                        printf("DONE!!!\n");
                        break;
                    }
                    else {
                        buf_information[i++] = buf[0];
                        //printf("buf_information = 0x%02X\n", (unsigned int)(buf_information[i-1] & 0xFF));
                    }
                    break;

                case STOP:
                    break;
            }
        }    
    }
    return (i + 1);
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
                    //printf("HelloClose\n");
                    if (buf[0] == FLAG_BLOCK) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break; 

                case FLAG_RCV:
                    printf("FLAG_Close\n");
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
                    printf("A_Close\n");
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
                    printf("C_Close\n");
                    printf("buf = 0x%02X,%d\n", (unsigned int)(buf[0] & 0xFF), bytes);
                    printf("a_block: %p\n", a_block);
                    printf("c_block: %p\n", c_block);
                    printf("xor gate: %p\n", a_block ^ c_block);
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
                    printf("BCC_OK_Close\n");
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
