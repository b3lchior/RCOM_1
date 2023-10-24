// Link layer protocol implementation

#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

int timeout = 0;
int nRetransmissions = 0;
int alarmTriggered = FALSE;
int alarmCounter = 0;

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters){
    int fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY); // opening the serial port

    if(fd < 0){
        perror("Error");
        return -1;
    }

    struct termios oldtio, newtio;

    if(tcgetattr(fd, &oldtio) == -1){ // reads the current settings of the serial port and stores them in oldtio
        perror("Error");
        return -1;
    }

    // setting the new serial port settings

    bzero(&newtio, sizeof(newtio)); // clears the newtio struct by filling it with zeros
    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD; 
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME]= 0;
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);

    if(tcsetattr(fd, TCSANOW, &newtio) == -1){ // sets the new serial port settings
        perror("Error");
        return -1;
    }

    unsigned char byte;
    timeout = connectionParameters.timeout;
    nRetransmissions = connectionParameters.nRetransmissions;

    if(connectionParameters.role == LlTx){
        (void) signal(SIGALRM, alarmHandler); // instala o handler definido em atende() para o sinal SIGALRM
    }
    while(connectionParameters.nRetransmissions != 0 &&  state)

}

void alarmHandler(){
    alarmTriggered = TRUE;
    alarmCounter++;
}



////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}
