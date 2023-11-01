// Main file of the serial port project.
// NOTE: This file must not be changed.

#include <stdio.h>
#include <stdlib.h>

#include "include/link_layer.h"
#include "include/application_layer.h"

#define N_TRIES 3
#define TIMEOUT 2

extern int state;

// Arguments:
//   $1: /dev/ttySxx
//   $2: tx | rx
//   $3: filename
int main(int argc, char *argv[])
{   
    if (argc < 4) {
        printf("Usage: %s /dev/ttySxx tx|rx filename\n", argv[0]);
        exit(1);
    }

    char *serialPort = argv[1];
    char *role = argv[2];
    char *filename = argv[3];
    
    printf("Starting link-layer protocol application\n"
           "  - Serial port: %s\n"
           "  - Role: %s\n"
           "  - Baudrate: %d\n"
           "  - Number of tries: %d\n"
           "  - Timeout: %d\n"
           "  - Filename: %s\n",
           serialPort,
           role,
           BAUDRATE,
           N_TRIES,
           TIMEOUT,
           filename);
    
    if (strcmp(role, "tx") == 0) {
        if (sendFile(argv[1], filename, BAUDRATE, N_TRIES, TIMEOUT) < 0) exit(-1);
    }
    else if(strcmp(role, "rx") == 0) {
        if (readFile(argv[1], filename, BAUDRATE, N_TRIES, TIMEOUT) < 0) exit(-1);
    }
    else exit(-1);

    return 0;
}