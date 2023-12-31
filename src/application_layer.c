#include "application_layer.h"

unsigned char* dataPackConstructor(unsigned char* message, int bufSize) {

	unsigned char* data_package = (unsigned char*) malloc(bufSize + 4);
    unsigned int l1 = bufSize % 256;
    unsigned int l2 = bufSize / 256;

    data_package[0] = C_DATA;
    data_package[1] = l2;
    data_package[2] = l1;

    for (int i = 0; i < bufSize; i++) {
        data_package[i + 3] = message[i];
    }

    return data_package;
}


int controlPackConstructor(unsigned char startEnd, unsigned char *packet, int fileSize, char *fileName) {
	packet[0] = startEnd;

    packet[1] = 0; // Type of file size is 0

    int len = 0;
    int curFileSize = fileSize;

    // File size To TLV form
    while (curFileSize > 0) {
        len++;

        int div = curFileSize / 256;
        int rest = curFileSize % 256;
        
        for (int i = 2 + len; i > 3; i--)
            packet[i] = packet[i - 1];

        packet[3] = (unsigned char) rest;

        curFileSize = div;
    }

    packet[2] = (unsigned char) len;

    packet[len + 3] = 1; // Type of file name is 1

    // File name To TLV form
    int fileNameBegin = len + 5; 
    int fileNameSize = strlen(fileName) + 1; // +1 para o '\0'

    packet[len + 4] = (unsigned char) fileNameSize;

    for (int j = 0; j < fileNameSize; j++) { 
        packet[fileNameBegin + j] = fileName[j];
    }

    unsigned int totalLen = 3 + len + 2 + fileNameSize;

    return totalLen;
}


int dataPackReader(unsigned char *packet, unsigned char *message) {
    if (packet[0] != C_DATA) return -1;

    int messageSize = 256 * packet[1] + packet[2];

    for (int i = 0; i < messageSize; i++) {
        message[i] = packet[i + 3];
    }

    return 0;
}


int controlPackReader(unsigned char *packet, int* fileSize, char *fileName) {
    int lenFileSize;

    if (packet[0] != C_START && packet[0] != C_END) return -1;

    if (packet[1] == 0) { // Type of file size is 0

        *fileSize = 0;
        lenFileSize = (int) packet[2];

        for (int i = 3; i < lenFileSize + 3; i++){
            *fileSize = *fileSize * 256 + (int) packet[i];
        }
    } 
    else return -1;

    int fileNameBegin = lenFileSize + 5;
    int lenFileName;

    if (packet[fileNameBegin - 2] == 1) { // Type of file name is 1

        lenFileName = (int) packet[fileNameBegin - 1];

        for (int i = 0; i < lenFileName; i++){
            fileName[i] = packet[fileNameBegin + i];
        }
    }
    else return -1;

    return 0;
}


int sendFile(char* serialPort, char* fileName, int baudRate, int nRetransmissions, int timeout) {
    if (fileName == NULL || serialPort == NULL) return -1;

    FILE* file;
    file = fopen(fileName, "r");
    if (file == NULL) return -1;

    LinkLayer ll;
    ll.role = LlTx;
    strncpy(ll.serialPort, serialPort, sizeof(ll.serialPort) - 1);
    ll.serialPort[sizeof(ll.serialPort) - 1] = '\0';
    ll.baudRate = baudRate;
    ll.nRetransmissions = nRetransmissions;
    ll.timeout = timeout;

    int fd = llopen(ll);
    if (fd < 0) return -1;

    unsigned char packet[MAX_SIZE];
    int fileSize = (int) getFileSize(file);

    /*-----------------------------------START-------------------------------------------*/

    int packetLen = controlPackConstructor(C_START, packet, fileSize, fileName);

    if (llwrite(fd, packet, packetLen) < 0) {
        fclose(file);
        return -1;
    } 

    unsigned char message[MAX_SIZE];
    unsigned int lenRead;
    unsigned char *data_package;

    while (TRUE) {
        lenRead = fread(message, sizeof(unsigned char), MAX_SIZE, file);
        if (lenRead != MAX_SIZE) {
            if (feof(file)) {
                data_package = dataPackConstructor(message, lenRead);
                lenRead += 3;
                
                if (llwrite(fd, data_package, lenRead) < 0) {
                    fclose(file);
                    return -1;
                } 
                break;
            } 
            else return -1;
        }
        data_package = dataPackConstructor(message, lenRead);
        lenRead += 3;

        if (llwrite(fd, data_package, lenRead) < 0) {
            fclose(file);
            return -1;
        }
    }

    packetLen = controlPackConstructor(C_END, packet, fileSize, fileName);

    /*------------------------------------END--------------------------------------------*/

    if (llwrite(fd, packet, packetLen) < 0) {
        fclose(file);
        return -1;
    }

    if (llclose(fd) < 0) {
        fclose(file);
        return -1;
    }

    if (fclose(file) != 0) return -1;

    return 0;
}


int readFile(char* serialPort, char *fileName, int baudRate, int nRetransmissions, int timeout) {
    if (fileName == NULL) return -1;

    LinkLayer ll;
    ll.role = LlRx;
    strncpy(ll.serialPort, serialPort, sizeof(ll.serialPort) - 1);
    ll.serialPort[sizeof(ll.serialPort) - 1] = '\0';
    ll.baudRate = baudRate;
    ll.nRetransmissions = nRetransmissions;
    ll.timeout = timeout;

    int fd = llopen(ll);
    if (fd < 0) return -1;

    unsigned char packet[(MAX_SIZE + 6)*2 + 6];
    int packetSize = llread(fd, packet);

    if (packetSize < 0) return -1;

    int fileSize = 0;

    if (packet[0] == C_START) {
        if (controlPackReader(packet, &fileSize, fileName) < 0) return -1;
    }
    else return -1;

    FILE* file;
    file = fopen("penguin-received.gif", "w");
    if (file == NULL) return -1;

    unsigned char message[MAX_SIZE];   

    while (TRUE) {
        packetSize = llread(fd, packet);
        if (packetSize < 0) return -1;

        if (packet[0] == C_DATA) {
            if (dataPackReader(packet, message) < 0) return -1;

            int messageLen = packetSize - 3;

            if (fwrite(message, sizeof(unsigned char), messageLen, file) != messageLen) return -1;
        }
        else if (packet[0] == C_END) break;
        else return -1;
    }

    int fileSize1;
    char fileName1[255];

    if (controlPackReader(packet, &fileSize1, fileName1) < 0) return -1;

    if (strcmp(fileName, fileName1) != 0) return -1;

    if (fileSize != fileSize1) return -1;
    
    if (llclose(fd) < 0){
        fclose(file);
        return -1;
    }

    if (fclose(file) != 0) return -1;

    return 0;
}

long int getFileSize(FILE *file) {
    long int size;
    long int currentPos = ftell(file);
    
    fseek(file, 0, SEEK_END);
    
    size = ftell(file);
    
    fseek(file, currentPos, SEEK_SET);
    
    return size;
}