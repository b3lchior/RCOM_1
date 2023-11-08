// Link layer protocol implementation

#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1

#define FLAG 0x7E
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07
#define C_DISC 0x0B
#define A_CLOSE 0x01
#define ESC 0x7D

#define BAUDRATE 38400  

#define C_N(Ns) (Ns << 6)
#define C_RR(Nr) ((Nr << 7) | 0x05)
#define C_REJ(Nr) ((Nr << 7) | 0x01)

int timeout = 0;
int nRetransmissions = 0;
int alarmEnabled = TRUE;
int alarmCount = 0;
int retries = 0;
int fd;

unsigned char tramaTx = 0;
unsigned char tramaRx = 1;

void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
}

int sendFrame(unsigned char x, unsigned char y){
    unsigned char frame[5] = {FLAG, x, y, x^y, FLAG};
    return write(fd, frame, 5);
}

unsigned char readControlFrame(){

    unsigned char byte, cField = 0;
    int state = 0;
    
    while (state != 5 && alarmEnabled == TRUE) {  
        if (read(fd, &byte, 1) > 0 || 1) {
            switch (state) {
                case 0: // START
                    if (byte == FLAG) state = 1;
                    break;
                case 1: // FLAG_RCV
                    if (byte == A) state = 2;
                    else if (byte != FLAG) state = 0;
                    break;
                case 2: // A_RCV
                    if (byte == C_RR(0) || byte == C_RR(1) || byte == C_REJ(0) || byte == C_REJ(1) || byte == C_DISC){
                        state = 3;
                        cField = byte;   
                    }
                    else if (byte == FLAG) state = 1;
                    else state = 0;
                    break;
                case 3: // C_RCV
                    if (byte == (A ^ cField)) state = 4;
                    else if (byte == FLAG) state = 1;
                    else state = 0;
                    break;
                case 4: // BCC_OK
                    if (byte == FLAG){
                        state = 5;
                    }
                    else state = 0;
                    break;
                default: 
                    break;
            }
        } 
    } 
    return cField;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {

    printf("Testing LLOPEN.\n");

    int state = 0; // START

    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY); 
    if(fd < 0){
        printf("Error opening the serial port.\n");
        return -1;
    }

    struct termios oldtio, newtio;

    if(tcgetattr(fd, &oldtio) == -1){ 
        printf("Error.");
        return -1;
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag =  BAUDRATE | CS8 | CLOCAL | CREAD; 
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME]= 0;
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);

    if(tcsetattr(fd, TCSANOW, &newtio) == -1){
        printf("Error.");
        return -1;
    }

    unsigned char byte;
    timeout = connectionParameters.timeout;
    nRetransmissions = connectionParameters.nRetransmissions;

    if(connectionParameters.role == LlTx) {

        (void) signal(SIGALRM, alarmHandler);
        retries = nRetransmissions;
        
        while(retries != 0 && state != 5)  {
            
            sendFrame(A, C_SET);
            alarm(connectionParameters.timeout);
            alarmEnabled = TRUE;

            while (alarmEnabled == TRUE && state != 5) {

                if (read(fd, &byte, 1) > 0) {
                    switch (state) {
                        case 0: // START
                            if (byte == FLAG) state = 1;
                            break;
                        case 1: // FLAG_RCV
                            if (byte == A_CLOSE) state = 2;
                            else if (byte != FLAG) state = 0;
                            break;
                        case 2: // A_RCV
                            if (byte == C_UA) state = 3;
                            else if (byte == FLAG) state = 1;
                            else state = 0;
                            break;
                        case 3: // C_RCV
                            if (byte == (A_CLOSE ^ C_UA)) state = 4;
                            else if (byte == FLAG) state = 1;
                            else state = 0;
                            break;
                        case 4: // BCC_OK
                            if (byte == FLAG) state = 5;
                            else state = 0;
                            break;
                        default:
                            break;
                    }
                }
            }
            retries--;
        }
    
        if (state != 5) return -1;
    }

    else if (connectionParameters.role == LlRx) {

        while (state != 5) { // STOP
            if (read(fd, &byte, 1) > 0) {
                switch (state) {
                    case 0: // START
                        if (byte == FLAG) state = 1;
                        break;
                    case 1: // FLAG_RCV
                        if (byte == A) state = 2;
                        else if (byte != FLAG) state = 0;
                        break;
                    case 2: // A_RCV
                        if (byte == C_SET) state = 3;
                        else if (byte == FLAG) state = 1;
                        else state = 0;
                        break;
                    case 3: // C_RCV
                        if (byte == (A ^ C_SET)) state = 4;
                        else if (byte == FLAG) state = 1;
                        else state = 0;
                        break;
                    case 4: // BCC1_OK
                        if (byte == FLAG) state = 5;
                        else state = 0;
                        break;
                    default: 
                        break;
                }
            }
        }
        sendFrame(A_CLOSE, C_UA);
    }

    else return -1;
    
    return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char* buf, int bufSize) {

    printf("Testing LLWRITE.\n");
    
    int frameSize = 6+bufSize;
    unsigned char *frame = (unsigned char *) malloc(frameSize);
    frame[0] = FLAG;
    frame[1] = A;
    frame[2] = C_N(tramaTx);
    frame[3] = frame[1] ^ frame[2];

    memcpy(frame+4,buf, bufSize);
    unsigned char BCC2 = buf[0];

    for (unsigned i = 1 ; i < bufSize ; i++) BCC2 ^= buf[i];

    int j = 4;
    for (unsigned i = 0 ; i < bufSize ; i++) {
        if(buf[i] == FLAG || buf[i] == ESC) {
            frame = realloc(frame,++frameSize);
            frame[j++] = ESC;
        }
        frame[j++] = buf[i];
    }

    frame[j++] = BCC2;
    frame[j++] = FLAG;

    int currentTransmition = 0;
    int rejected = 0, accepted = 0;

    while (currentTransmition < retries) { 
        alarmEnabled = TRUE;
        alarm(timeout);
        rejected = 0;
        accepted = 0;

        while (alarmEnabled == TRUE && !rejected && !accepted) {

            write(fd, frame, j);
            unsigned char result = readControlFrame(fd);
            
            if(!result){
                continue;
            }
            else if(result == C_REJ(0) || result == C_REJ(1)) {
                rejected = 1;
            }
            else if(result == C_RR(0) || result == C_RR(1)) {
                accepted = 1;
                tramaTx = (tramaTx+1) % 2;
            }
            else continue;

        }
        if (accepted) break;
        currentTransmition++;
    }
    
    free(frame);
    if(accepted) return frameSize;
    else{
        llclose(fd);
        return -1;
    }
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    printf("Testing LLREAD.\n");
    unsigned char byte, cField;
    int i = 0;
    int state = 0; // START

    while (state != 6) {  // STOP
        if (read(fd, &byte, 1) > 0) {
            switch (state) {
                case 0: // START
                    if (byte == FLAG) state = 1;
                    break;
                case 1: // FLAG_RCV
                    if (byte == A) state = 2;
                    else if (byte != FLAG) state = 0;
                    break;
                case 2: // A_RCV
                    if (byte == C_N(0) || byte == C_N(1)){
                        state = 3;
                        cField = byte;   
                    }
                    else if (byte == FLAG) state = 1;
                    else if (byte == C_DISC) {
                        sendFrame(A_CLOSE, C_DISC);
                        return 0;
                    }
                    else state = 0;
                    break;
                case 3: // C_RCV
                    if (byte == (A ^ cField)) state = 4;
                    else if (byte == FLAG) state = 1;
                    else state = 0;
                    break;
                case 4: // READING_DATA
                    if (byte == ESC) state = 5;
                    else if (byte == FLAG){
                        unsigned char bcc2 = packet[i-1];
                        i--;
                        packet[i] = '\0';
                        unsigned char acc = packet[0];

                        for (unsigned int j = 1; j < i; j++)
                            acc ^= packet[j];

                        if (bcc2 == acc){
                            state = 6;
                            sendFrame(A, C_RR(tramaRx));
                            tramaRx = (tramaRx + 1)%2;
                            return i; 
                        }
                        else{
                            printf("Error: retransmition\n");
                            sendFrame(A, C_REJ(tramaRx));
                            return -1;
                        };

                    }
                    else{
                        packet[i++] = byte;
                    }
                    break;
                case 5: // DATA_FOUND_ESC
                    state = 4;
                    if (byte == ESC || byte == FLAG) packet[i++] = byte;
                    else{
                        packet[i++] = ESC;
                        packet[i++] = byte;
                    }
                    break;
                default: 
                    break;
            }
        }
    }
    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    printf("Testing LLCLOSE.\n");
    int state = 0; // START
    unsigned char byte;
    (void) signal(SIGALRM, alarmHandler);
    
    while (retries != 0 && state != 5) {
                
        sendFrame(A, C_DISC);
        alarm(timeout);
        alarmEnabled = TRUE;
                
        while (alarmEnabled == TRUE && state != 5) {
            if (read(fd, &byte, 1) > 0) {
                switch (state) {
                    case 0: // START
                        if (byte == FLAG) state = 1;
                        break;
                    case 1: // FLAG_RCV
                        if (byte == A_CLOSE) state = 2;
                        else if (byte != FLAG) state = 0;
                        break;
                    case 2: // A_RCV
                        if (byte == C_DISC) state = 3;
                        else if (byte == FLAG) state = 1;
                        else state = 0;
                        break;
                    case 3: // C_RCV
                        if (byte == (A_CLOSE ^ C_DISC)) state = 4;
                        else if (byte == FLAG) state = 1;
                        else state = 0;
                        break;
                    case 4: // BCC_OK
                        if (byte == FLAG) state = 5;
                        else state = 0;
                        break;
                    default: 
                        break;
                }
            }
        } 
        retries--;
    }

    if (state != 5) return -1;

    sendFrame(A, C_UA);
    return close(fd);
}
