// Link layer protocol implementation

#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

extern int alarmCount;

struct termios oldtio;

int connectionType;
int tries;
int timeout;
int sendReceiveValidate;
int fullLength = 0;
u_int8_t responseByte;
unsigned char frameNumberByte = 0x00;

extern int alarmEnabled;

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(connectionParameters.serialPort);
        return -1;
    }

    connectionType = connectionParameters.role;
    tries = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;
    sendReceiveValidate = 0;

    struct termios newtio;
    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        return -1;
    }
    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }
    printf("New termios structure set\n");

    // Create string to send
    unsigned char buf_set[BUF_SIZE] = {0};
    buf_set[0] = 0x7E;
    buf_set[1] = 0x03;
    buf_set[2] = 0x03;
    buf_set[3] = buf_set[1] ^ buf_set[2];
    buf_set[4] = 0x7E;

    unsigned char buf_s[BUF_SIZE] = {0};
    unsigned char buf_ua[BUF_SIZE] = {0};

    (void)signal(SIGALRM, alarmHandler);

    int alarm;

    switch (connectionParameters.role) {
        case LlTx:
            alarm = setStateMachineTransmitter(fd, buf_set, buf_ua, 0x03, C_BLOCK_UA); // envia set e analisa ua
            if (alarm == tries) return -1;
            break;
 
        case LlRx:
            printf("Waiting for SET Trama\n");
            setStateMachineReceiver(fd, buf_s, 0x03, C_BLOCK_UA); // analisa set e envia ua
            printf("Sent a UA Trama\n");
            break;

        default:
            return -1;
    }

    sleep(1);

    return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(int fd, const unsigned char *buf, int bufSize) {
    if (sendReceiveValidate == 0) {
        frameNumberByte = 0x00;
    }
    else if (sendReceiveValidate == 1) {
        frameNumberByte = 0x40;
    }
    
    unsigned char buf_inf[bufSize];

    buf_inf[0] = FLAG_BLOCK;
    buf_inf[1] = 0x03;
    buf_inf[2] = frameNumberByte;
    buf_inf[3] = buf_inf[1] ^ buf_inf[2];

    for (int i = 0; i < bufSize; i++) {
        buf_inf[i + 4] = buf[i];
    }

    unsigned char bcc2 = buf[0];

    for (int i = 1; i < bufSize; i++) {
        bcc2 ^= buf[i];
    }

    buf_inf[3 + bufSize + 1] = bcc2;
    buf_inf[3 + bufSize + 2] = FLAG_BLOCK;

    fullLength = byte_stuffing(buf_inf, bufSize);

    if (fullLength < 0) {
        sleep(1);

        if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
            perror("tcsetattr");
            return -1;
        }
    }

    int bytesWritten = 0;
    int sentData = FALSE;
    alarmEnabled = FALSE;
    int resend = 1;

    (void)signal(SIGALRM, alarmHandler);
    
    while (!sentData) {
        resend = setStateMachineWrite(fd, buf_inf, fullLength, &bytesWritten, &frameNumberByte); // envia a Trama I e aguarda pela trama S do receiver. Recebendo-a, verifica se está em ordem

        if (resend == -1) return -1;

        if (resend == 1) { // Significa que recebeu REJ
            printf("Received REJ. Sending another Trama. RESEND: %d\n", resend); 
        }

        else if (resend == 0) { // Significa que enviou trama I com sucesso. Termina o ciclo
            sentData = TRUE;
        }
    }

    switch (sendReceiveValidate) { // trocar o frame number após receber a resposta
        case 0:
            sendReceiveValidate = 1;
            break;
        case 1: 
            sendReceiveValidate = 0;
            break;
        
        default:
            return -1;
    }  

    return bytesWritten;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(int fd, unsigned char *packet) {
    int nbytes;
    int bytesInfo;
    int bufferIsFull = FALSE;

    unsigned char buf_information[(MAX_SIZE + 6)*2 + 6];
    unsigned char buf_inf[(MAX_SIZE + 6)*2 + 6];

    while (!bufferIsFull) {
    
        bytesInfo = setStateMachineReceiverInf(fd, buf_inf, buf_information, A_BLOCK_INF_TRANS); // aguarda trama I e cria-a na variável buf_information. Retorna o tamanho do data packet + bcc2, w/stuffing

        printf("Received I Trama. Bytes Received: %d\n", bytesInfo);

        nbytes = byte_destuffing(buf_information, bytesInfo);

        if (nbytes < 0) {
            sleep(1);

            if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
                perror("tcsetattr");
                return -1;
            }
        }

        int receiveSendByte;
        if (buf_information[2] == 0x00) { // retira o número do frame da trama I enviada
            receiveSendByte = 0;
        }
        else if (buf_information[2] == 0x40) {
            receiveSendByte = 1;
        }
        unsigned char bcc2 = buf_information[4];

        for (int i = 1; i < nbytes - 6; i++) {
            bcc2 ^= buf_information[i + 4];
        }

        if (buf_information[nbytes - 2] == bcc2) { // verificar que bcc2 está correto
            if (receiveSendByte != sendReceiveValidate) { // trama duplicada. Discartar informação
                if (receiveSendByte == 0) { // ignora dados da trama
                    responseByte = 0x85; // RR1
                    sendReceiveValidate = 1;
                }
                else {
                    responseByte = 0x05; // RR0
                    sendReceiveValidate = 0;
                }
            }
            else { // nova trama
                for (int i = 0; i < nbytes - 6; i++) { 
                    packet[i] = buf_information[4 + i]; // passa a informação para a packet
                }

                bufferIsFull = TRUE;

                if (receiveSendByte == 0) {
                    responseByte = 0x85;
                    sendReceiveValidate = 1;
                }
                else {
                    responseByte = 0x05;
                    sendReceiveValidate = 0;
                }
            }
        }
        else { // if bcc2 is not correct
            if (receiveSendByte != sendReceiveValidate) { // trama duplicada
                if (receiveSendByte == 0) { // ignora frama data
                    responseByte = 0x85; // RR1
                    sendReceiveValidate = 1;
                }
                else {
                    responseByte = 0x05; // RR0
                    sendReceiveValidate = 0;
                }
            }
            else { // new trama
                if (receiveSendByte == 0) { // ignora frame data por causa do erro
                    responseByte = 0x01; // REJ0
                    sendReceiveValidate = 0;
                }
                else {
                    responseByte = 0x81; // REJ1
                    sendReceiveValidate = 1;
                }
            }
        }

        // create S trama
        unsigned char buf_super[BUF_SIZE + 1];

        buf_super[0] = FLAG_BLOCK;
        buf_super[1] = 0x03; 
        buf_super[2] = responseByte;
        buf_super[3] = buf_super[1] ^ buf_super[2];
        buf_super[4] = FLAG_BLOCK;
        buf_super[5] = '\n';
        
        int bytesWrite = write(fd, buf_super, BUF_SIZE);
        printf("Supervision trama has been sent\n");

        if (bytesWrite < 0) {
            sleep(1);

            if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
                perror("tcsetattr");
                return -1;
            }
        }
    }

    fullLength = nbytes - 6;

    return fullLength; 
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int fd)
{
    alarmCount = 0;
    unsigned char buf_disc_trans[BUF_SIZE + 1] = {0};

    unsigned char buf_trans[BUF_SIZE + 1] = {0};
    buf_trans[0] = FLAG_BLOCK;
    buf_trans[1] = A_BLOCK_DISC_TRANS;
    buf_trans[2] = C_BLOCK_DISC;
    buf_trans[3] = (A_BLOCK_DISC_TRANS ^ C_BLOCK_DISC);
    buf_trans[4] = FLAG_BLOCK;
    buf_trans[5] = '\n';
   
    unsigned char buf_rec[BUF_SIZE + 1] = {0};
    unsigned char buf_ua[BUF_SIZE + 1] = {FLAG_BLOCK, A_BLOCK_UA, C_BLOCK_UA, 0x01 ^ 0x07, FLAG_BLOCK, '\n'};
    int end = FALSE;

    (void)signal(SIGALRM, alarmHandler);

    int countT = 0;
    int countR = 0;
    while (!end) { 
        if (connectionType == TRANSMITTER) {
            if (countT > 0) {
                printf("Sent the last Disconnect trama\n");
                write(fd, buf_ua, BUF_SIZE);
                break;
            } 
            else {
                printf("Sent a Disconnect trama\n");
                if (setStateMachineTransmitter(fd, buf_trans, buf_disc_trans, 0x01, C_BLOCK_DISC) >= tries) return -1;
                countT++;

                if (buf_trans[2] != C_BLOCK_DISC) {
                    printf("Not a Disconnect trama\n");
                    countT = 0;
                }
            }
        }

        else if (connectionType == RECEIVER) {
            if (countR > 0) { 
                printf("Waiting for UA response\n");
                setStateMachineReceiverDisc(fd, buf_rec, A_BLOCK_UA, C_BLOCK_UA);
                printf("Received UA trama\n");
                end = TRUE;
                break;
            }
            else {
                setStateMachineReceiverDisc(fd, buf_rec, A_BLOCK_DISC_TRANS, C_BLOCK_DISC);
                printf("Received a Disconnect trama\n");
                countR++;
                buf_rec[0] = FLAG_BLOCK;
                buf_rec[1] = 0x01;
                buf_rec[2] = C_BLOCK_DISC;
                buf_rec[3] = buf_rec[1] ^ buf_rec[2];
                buf_rec[4] = FLAG_BLOCK;
                buf_rec[5] = '\n';

                write(fd, buf_rec, BUF_SIZE);

                printf("Sending Disconnect Trama\n");

                if (buf_rec[2] != C_BLOCK_DISC) {
                    printf("Not a Disconnect trama");
                    countR = 0;
                }
            }
        }
        else {
            countR = 0;
            countT = 0;
            }

    }

    sleep(1);
    
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }

    if (close(fd) < 0) {
        perror("close");
        return -1;
    }

    return fd;
}

int byte_stuffing(unsigned char* new_message, int bufSize) {
    unsigned char tmp[bufSize + 6]; // tamanho da packet + 6 bytes para o frame header e tail

    for (int i = 0; i < bufSize + 6 ; i++) {
        tmp[i] = new_message[i];
    }

    int newLength = 4;

    for (int i = 4; i < (bufSize + 6); i++) {
        if (tmp[i] == FLAG_BLOCK && i != (bufSize + 5)) {
            new_message[newLength] = 0x7D;
            new_message[newLength + 1] = 0x5E;
            newLength = newLength + 2;
        }

        else if (tmp[i] == 0x7D && i != (bufSize + 5)) {
            new_message[newLength] = 0x7D;
            new_message[newLength + 1] = 0x5D;
            newLength = newLength + 2;
        }

        else {
            new_message[newLength] = tmp[i];
            newLength++;
        }
    }

    return newLength;
}

int byte_destuffing(unsigned char* new_message, int bufSize) {
    unsigned char tmp[bufSize + 5];

    for (int i = 0; i < bufSize + 5; i++) {
        tmp[i] = new_message[i];
    }

    int newLength = 4;

    for (int i = 4; i < bufSize + 5; i++) {
        if (tmp[i] == 0x7D) {
            if (tmp[i + 1] == 0x5D) {
                new_message[newLength] = 0x7D;
            }
            else if (tmp[i + 1] == 0x5E) {
                new_message[newLength] = FLAG_BLOCK;
            }
            i++;
            newLength++;
        }

        else {
            new_message[newLength] = tmp[i];
            newLength++;
        }
    }

    return newLength;
}